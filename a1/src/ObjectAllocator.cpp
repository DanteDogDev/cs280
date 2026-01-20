#include "ObjectAllocator.h"

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
  m.stats.PageSize_ = page_block_size + mem_block_size * config.ObjectsPerPage_;
  NewPage();
}

void ObjectAllocator::NewPage() {
  m.stats.PagesInUse_++;
  m.stats.FreeObjects_ += m.config.ObjectsPerPage_;

  unsigned char * memory;
  try {
    memory = new unsigned char[m.pageBlockSize + m.memBlockSize * m.config.ObjectsPerPage_];
  } catch (...) {
    throw;
  }

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
      memory[offset++] = ALLOCATED_PATTERN;
    }
    for (size_t j = 0; j < m.config.PadBytes_; ++j) {
      memory[offset++] = PAD_PATTERN;
    }
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

  if (m.pageList) {
    page->Next = m.pageList;
  }
  m.pageList = page;
}

ObjectAllocator::~ObjectAllocator() { }

void* ObjectAllocator::Allocate(const char* label) {
  m.stats.Allocations_++;
  m.stats.ObjectsInUse_++;
  if (m.stats.ObjectsInUse_ > m.stats.MostObjects_) {
    m.stats.MostObjects_ = m.stats.ObjectsInUse_;
  }
  return (void*)label;
}

void ObjectAllocator::Free(void* label) {
  m.stats.Deallocations_++;
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
