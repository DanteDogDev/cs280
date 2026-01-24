#include "ObjectAllocator.h"

#include <cstdint>
#include <cstring>

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config) {
  size_t page_block_size = sizeof(void*) + config.LeftAlignSize_ + config.HBlockInfo_.size_ + config.InterAlignSize_ + config.PadBytes_;
  size_t mem_block_size = ObjectSize + config.PadBytes_ + config.Alignment_ + config.HBlockInfo_.size_ + config.PadBytes_;
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
  m.stats.PageSize_ = page_block_size + mem_block_size * config.ObjectsPerPage_ - config.PadBytes_ - config.HBlockInfo_.size_;
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

  // Page Block
  size_t pb_offset = sizeof(GenericObject*);    // page block offset
  auto page = reinterpret_cast<GenericObject*>(memory);
  page->Next = nullptr;
  for (size_t i = 0; i < m.config.LeftAlignSize_; ++i) {
    memory[pb_offset++] = ALIGN_PATTERN;
  }
  for (size_t i = 0; i < m.config.HBlockInfo_.size_; ++i) {
    memory[pb_offset++] = 0;
  }
  for (size_t i = 0; i < m.config.InterAlignSize_; ++i) {
    memory[pb_offset++] = ALIGN_PATTERN;
  }
  for (size_t i = 0; i < m.config.PadBytes_; ++i) {
    memory[pb_offset++] = PAD_PATTERN;
  }

  // Memory Block
  for (size_t i = 0; i < m.config.ObjectsPerPage_; ++i) {
    size_t offset = pb_offset + m.memBlockSize * i;
    for (size_t j = 0; j < m.stats.ObjectSize_; ++j) {
      memory[offset++] = UNALLOCATED_PATTERN;
    }
    for (size_t j = 0; j < m.config.PadBytes_; ++j) {
      memory[offset++] = PAD_PATTERN;
    }
    if (i != m.config.ObjectsPerPage_ - 1) {
      for (size_t j = 0; j < m.config.Alignment_; ++j) {
        memory[offset++] = ALIGN_PATTERN;
      }
      for (size_t j = 0; j < m.config.HBlockInfo_.size_; ++j) {
        memory[offset++] = 0;
      }
      for (size_t j = 0; j < m.config.PadBytes_; ++j) {
        memory[offset++] = PAD_PATTERN;
      }
    }
  }

  for (size_t i = 0; i < m.config.ObjectsPerPage_; ++i) {
    size_t offset = pb_offset + m.memBlockSize * i;
    auto* obj = reinterpret_cast<GenericObject*>(&memory[offset]);
    obj->Next = m.freeList;
    m.freeList = obj;
    m.stats.FreeObjects_++;
  }

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
  return obj;
}

void ObjectAllocator::Free(void* label) {
  // {
  //   bool found = false;
  //   auto* page_list = m.pageList;
  //   auto addr_label = reinterpret_cast<std::uintptr_t>(label);
  //   while (page_list) {
  //     auto addr_begin = reinterpret_cast<std::uintptr_t>(page_list);
  //     auto addr_end = addr_begin + m.stats.PageSize_;
  //     if (addr_begin < addr_label && addr_label < addr_end) {
  //       auto relative = addr_label - addr_begin;
  //       relative -= m.pageBlockSize;
  //       if (relative % m.memBlockSize == 0) {
  //         found = true;
  //         break;
  //       } else {
  //         break;
  //       }
  //     }
  //     page_list = page_list->Next;
  //   }
  //   if (not found) {
  //     throw OAException(OAException::E_BAD_BOUNDARY, "Free memory not allocated in the object allocator");
  //   }
  // }
  {
    auto* free_list = m.freeList;
    while (free_list) {
      if (free_list == label) {
        throw OAException(OAException::E_MULTIPLE_FREE, "Double Free");
      }
      free_list = free_list->Next;
    }
  }
  m.stats.Deallocations_++;
  m.stats.FreeObjects_++;
  m.stats.ObjectsInUse_--;
  memset(label, FREED_PATTERN, m.stats.ObjectSize_);
  auto* obj = reinterpret_cast<GenericObject*>(label);
  obj->Next = m.freeList;
  m.freeList = obj;
  (void)label;
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

// void ObjectAllocator::allocate_new_page(void) { }
//
// void put_on_freelist(void* Object) {
//   (void)Object;
// }
