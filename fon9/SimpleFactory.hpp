// \file fon9/SimpleFactory.hpp
// \author fonwinz@gmail.com
#ifndef __fon9_SimpleFactory_hpp__
#define __fon9_SimpleFactory_hpp__
#include "fon9/SortedVector.hpp"
#include "fon9/CharVector.hpp"

namespace fon9 {

template <class Factory>
Factory SimpleFactoryRegister(StrView name, Factory factory) {
   using Key = CharVector;
   using FactoryMap = SortedVector<Key, Factory>;
   static FactoryMap FactoryMap_;
   if (!factory) {
      auto ifind = FactoryMap_.find(Key::MakeRef(name));
      return ifind == FactoryMap_.end() ? nullptr : ifind->second;
   }
   auto& ref = FactoryMap_.kfetch(Key::MakeRef(name));
   if (!ref.second)
      ref.second = factory;
   else
      fprintf(stderr, "SimpleFactoryRegister: dup name: %s\n", name.ToString().c_str());
   return ref.second;
}

template <class FactoryPark>
struct SimpleFactoryPark {
   static void SimpleRegister() {
   }

   template <class Factory, class... ArgsT>
   static void SimpleRegister(StrView name, Factory factory, ArgsT&&... args) {
      FactoryPark::Register(name, factory);
      SimpleRegister(std::forward<ArgsT>(args)...);
   }

   template <class... ArgsT>
   SimpleFactoryPark(ArgsT&&... args) {
      SimpleRegister(std::forward<ArgsT>(args)...);
   }

   SimpleFactoryPark() = delete;
};

} // namespaces
#endif//__fon9_SimpleFactory_hpp__
