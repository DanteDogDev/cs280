#include "ObjectAllocator.h"

#include <cstdint>
#include <cstring>

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config) {
  size_t page_block_size = sizeof(void*) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.InterAlignSize_ + config.PadBytes_;
  size_t mem_block_size = ObjectSize + config.PadBytes_ + config.InterAlignSize_ + config.HBlockInfo_.size_ + config.PadBytes_;
  // clang-format off
  m = {
    page_block_size,
    mem_block_size,
    config,
    OAStats(),
    nullptr,
    nullptr,
  };
  // clang-format on
  m.stats.ObjectSize_ = ObjectSize;
  m.stats.PageSize_ =
      page_block_size + mem_block_size * config.ObjectsPerPage_ - config.PadBytes_ - config.HBlockInfo_.size_ - config.InterAlignSize_;
  NewPage();
}

void ObjectAllocator::NewPage() {
  if (m.stats.PagesInUse_ == m.config.MaxPages_) {
    throw OAException(OAException::E_NO_PAGES, "Maximum Pages Has Been Reached");
  }
  unsigned char* memory;
  try {
    memory = new unsigned char[m.stats.PageSize_];
  } catch (...) { throw OAException(OAException::E_NO_MEMORY, "Out Of Memory"); }

  {
    size_t pb_offset = sizeof(GenericObject*);    // page block offset
    // Page Block
    memset(memory + pb_offset, ALIGN_PATTERN, m.config.LeftAlignSize_);
    pb_offset += m.config.LeftAlignSize_;
    memset(memory + pb_offset, 0, m.config.HBlockInfo_.size_);
    pb_offset += m.config.HBlockInfo_.size_;
    memset(memory + pb_offset, ALIGN_PATTERN, m.config.InterAlignSize_);
    pb_offset += m.config.InterAlignSize_;
    memset(memory + pb_offset, PAD_PATTERN, m.config.PadBytes_);
    pb_offset += m.config.PadBytes_;
  }

  // Memory Block
  for (size_t i = 0; i < m.config.ObjectsPerPage_; ++i) {
    size_t offset = m.pageBlockSize + m.memBlockSize * i;

    memset(memory + offset, UNALLOCATED_PATTERN, m.stats.ObjectSize_);
    offset += m.stats.ObjectSize_;
    memset(memory + offset, PAD_PATTERN, m.config.PadBytes_);
    offset += m.config.PadBytes_;
    if (i != m.config.ObjectsPerPage_ - 1) {
      memset(memory + offset, ALIGN_PATTERN, m.config.InterAlignSize_);
      offset += m.config.InterAlignSize_;
      memset(memory + offset, 0, m.config.HBlockInfo_.size_);
      offset += m.config.HBlockInfo_.size_;
      memset(memory + offset, PAD_PATTERN, m.config.PadBytes_);
      offset += m.config.PadBytes_;
    }
  }

  for (size_t i = 0; i < m.config.ObjectsPerPage_; ++i) {
    size_t offset = m.pageBlockSize + m.memBlockSize * i;
    auto* obj = reinterpret_cast<GenericObject*>(&memory[offset]);
    obj->Next = m.freeList;
    m.freeList = obj;
    m.stats.FreeObjects_++;
  }

  auto page = reinterpret_cast<GenericObject*>(memory);
  page->Next = nullptr;

  if (m.pageList) {
    page->Next = m.pageList;
  }
  m.pageList = page;
  m.stats.PagesInUse_++;
}

ObjectAllocator::~ObjectAllocator() {
  m.freeList = nullptr;
  while (m.pageList) {
    auto* temp = m.pageList;
    m.pageList = m.pageList->Next;
    delete[] temp;
  }
}

void* ObjectAllocator::Allocate(const char* label) {
  if (not m.freeList) {
    NewPage();
  }
  m.stats.Allocations_++;
  m.stats.FreeObjects_--;
  m.stats.ObjectsInUse_++;
  if (m.stats.ObjectsInUse_ > m.stats.MostObjects_) {
    m.stats.MostObjects_ = m.stats.ObjectsInUse_;
  }
  auto* obj = m.freeList;
  m.freeList = m.freeList->Next;
  memset(obj, ALLOCATED_PATTERN, m.stats.ObjectSize_);
  {
    auto header = reinterpret_cast<unsigned char*>(obj) - m.config.PadBytes_ - m.config.HBlockInfo_.size_;
    switch (m.config.HBlockInfo_.type_) {
      case OAConfig::hbBasic: {
        auto* alloc_num = (reinterpret_cast<unsigned int*>(header));
        header += sizeof(unsigned int);
        auto* flag = (reinterpret_cast<unsigned char*>(header));
        *alloc_num = m.stats.Allocations_;
        *flag = 1;
        break;
      }
      case OAConfig::hbExtended: {    // TODO:
        header += m.config.HBlockInfo_.additional_;
        auto* use_counter = (reinterpret_cast<unsigned short*>(header));
        header += sizeof(unsigned short);
        auto* alloc_num = (reinterpret_cast<unsigned*>(header));
        header += sizeof(unsigned int);
        auto* flag = (reinterpret_cast<unsigned char*>(header));
        *use_counter = *use_counter + 1;
        *alloc_num = m.stats.Allocations_;
        *flag = 1;
        break;
      }
      case OAConfig::hbExternal: {
        auto* ptr = (reinterpret_cast<MemBlockInfo**>(header));
        *ptr = new MemBlockInfo();
        (*ptr)->alloc_num = m.stats.Allocations_;
        (*ptr)->in_use = 1;
        (*ptr)->label = strdup(label);
        break;
      }
      case OAConfig::hbNone: {
        break;
      }
    }
  }
  return obj;
}

void ObjectAllocator::Free(void* label) {
  {
    bool found = false;
    auto* page_list = m.pageList;
    auto addr_label = reinterpret_cast<std::uintptr_t>(label);
    while (page_list) {
      auto addr_begin = reinterpret_cast<std::uintptr_t>(page_list);
      auto addr_end = addr_begin + m.stats.PageSize_;
      if (addr_begin < addr_label && addr_label < addr_end) {
        auto relative = addr_label - addr_begin;
        relative -= m.pageBlockSize;
        if (relative % m.memBlockSize == 0) {
          found = true;
          break;
        } else {
          break;
        }
      }
      page_list = page_list->Next;
    }
    if (not found) {
      throw OAException(OAException::E_BAD_BOUNDARY, "Free memory not allocated in the object allocator");
    }
  }
  {
    auto* free_list = m.freeList;
    while (free_list) {
      if (free_list == label) {
        throw OAException(OAException::E_MULTIPLE_FREE, "Double Free");
      }
      free_list = free_list->Next;
    }
  }
  memset(label, FREED_PATTERN, m.stats.ObjectSize_);
  auto* obj = reinterpret_cast<GenericObject*>(label);
  obj->Next = m.freeList;
  m.freeList = obj;
  m.stats.Deallocations_++;
  m.stats.FreeObjects_++;
  m.stats.ObjectsInUse_--;
  {
    auto header = reinterpret_cast<unsigned char*>(obj) - m.config.PadBytes_ - m.config.HBlockInfo_.size_;
    switch (m.config.HBlockInfo_.type_) {
      case OAConfig::hbBasic: {
        auto* alloc_num = (reinterpret_cast<unsigned int*>(header));
        header += sizeof(unsigned int);
        auto* flag = (reinterpret_cast<unsigned char*>(header));
        *alloc_num = 0;
        *flag = 0;
        break;
      }
      case OAConfig::hbExtended: {    // TODO:
        header += m.config.HBlockInfo_.additional_;
        // auto* use_counter = (reinterpret_cast<unsigned short*>(header));
        header += sizeof(unsigned short);
        auto* alloc_num = (reinterpret_cast<unsigned*>(header));
        header += sizeof(unsigned int);
        auto* flag = (reinterpret_cast<unsigned char*>(header));
        // *use_counter = *use_counter + 1;
        *alloc_num = 0;
        *flag = 0;
        break;
      }
      case OAConfig::hbExternal: {
        auto* ptr = (reinterpret_cast<MemBlockInfo**>(header));
        *ptr = new MemBlockInfo();
        (*ptr)->alloc_num = 0;
        (*ptr)->in_use = 0;
        free((*ptr)->label);
        (*ptr)->label = nullptr;
        break;
      }
      case OAConfig::hbNone: break;
    }
  }
  {
    auto left_pad = reinterpret_cast<unsigned char*>(obj) - m.config.PadBytes_;
    for (size_t i = 0; i < m.config.PadBytes_; ++i) {
      if (*left_pad != PAD_PATTERN) {
        throw OAException(OAException::E_CORRUPTED_BLOCK, "PadBytes_ on the left corrupted");
      }
      left_pad++;
    }
  }
  {
    auto right_pad = reinterpret_cast<unsigned char*>(obj) + m.stats.ObjectSize_;
    for (size_t i = 0; i < m.config.PadBytes_; ++i) {
      if (*right_pad != PAD_PATTERN) {
        throw OAException(OAException::E_CORRUPTED_BLOCK, "PadBytes_ on the right corrupted");
      }
      right_pad++;
    }
  }
}

unsigned ObjectAllocator::DumpMemoryInUse(DUMPCALLBACK fn) const {
  fn(0, 0);
  return 0;
}

unsigned ObjectAllocator::ValidatePages(VALIDATECALLBACK fn) const {
  fn(0, 0);
  return 0;
}

unsigned ObjectAllocator::FreeEmptyPages(void) {
  return 0;
}

bool ObjectAllocator::ImplementedExtraCredit(void) {
  return false;
}

void ObjectAllocator::SetDebugState(bool State) {
  (void)State;
}

const void* ObjectAllocator::GetFreeList(void) const {
  return m.freeList;
}

const void* ObjectAllocator::GetPageList(void) const {
  return m.pageList;
}

OAConfig ObjectAllocator::GetConfig(void) const {
  return m.config;
}

OAStats ObjectAllocator::GetStats(void) const {
  return m.stats;
}
