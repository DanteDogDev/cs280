#include "ObjectAllocator.h"

ObjectAllocator::ObjectAllocator(size_t ObjectSize, const OAConfig& config) {
  (void)ObjectSize;
  (void)config;
}

ObjectAllocator::~ObjectAllocator() { }

void* ObjectAllocator::Allocate(const char* label) {
  return (void*)label;
}

void ObjectAllocator::Free(void* label) {
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
  return 0;
}

const void* ObjectAllocator::GetPageList(void) const {
  return 0;
}

OAConfig ObjectAllocator::GetConfig(void) const {
  return {};
}

OAStats ObjectAllocator::GetStats(void) const {
  return {};
}

void ObjectAllocator::allocate_new_page(void) { }

void put_on_freelist(void* Object) {
  (void)Object;
}
