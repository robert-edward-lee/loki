////////////////////////////////////////////////////////////////////////////////
// The Loki Library
// Copyright (c) 2001 by Andrei Alexandrescu
// This code accompanies the book:
// Alexandrescu, Andrei. "Modern C++ Design: Generic Programming and Design 
//     Patterns Applied". Copyright (c) 2001. Addison-Wesley.
// Permission to use, copy, modify, distribute and sell this software for any 
//     purpose is hereby granted without fee, provided that the above copyright 
//     notice appear in all copies and that both that copyright notice and this 
//     permission notice appear in supporting documentation.
// The author or Addison-Wesley Longman make no representations about the 
//     suitability of this software for any purpose. It is provided "as is" 
//     without express or implied warranty.
////////////////////////////////////////////////////////////////////////////////

// Last update: March 05, 2001

#ifndef HIERARCHYGENERATORS_INC_
#define HIERARCHYGENERATORS_INC_

#include "Typelist.h"
#include "TypeTraits.h"
#include "EmptyType.h"

namespace Loki
{
////////////////////////////////////////////////////////////////////////////////
// class template GenScatterHierarchy
// Generates a scattered hierarchy starting from a typelist and a template
// Invocation (TList is a typelist, Model is a template of one arg):
// GenScatterHierarchy<TList, Model>
// The generated class inherits all classes generated by instantiating the 
// template 'Model' with the types contained in TList 
////////////////////////////////////////////////////////////////////////////////

    template <class TList, template <class> class Unit>
    class GenScatterHierarchy;
     
    template <class T1, class T2, template <class> class Unit>
    class GenScatterHierarchy<Typelist<T1, T2>, Unit>
        : public GenScatterHierarchy<T1, Unit>
        , public GenScatterHierarchy<T2, Unit>
    {
    public:
        typedef Typelist<T1, T2> TList;
        typedef GenScatterHierarchy<T1, Unit> LeftBase;
        typedef GenScatterHierarchy<T2, Unit> RightBase;
        template <typename T> struct Rebind
        {
            typedef Unit<T> Result;
        };
    };
     
    template <class AtomicType, template <class> class Unit>
    class GenScatterHierarchy : public Unit<AtomicType>
    {
        typedef Unit<AtomicType> LeftBase;
        template <typename T> struct Rebind
        {
            typedef Unit<T> Result;
        };
    };
    
    template <template <class> class Unit>
    class GenScatterHierarchy<NullType, Unit>
    {
        template <typename T> struct Rebind
        {
            typedef Unit<T> Result;
        };
    };
     
////////////////////////////////////////////////////////////////////////////////
// function template Field
// Accesses a field in an object of a type generated with GenScatterHierarchy
// Invocation (obj is an object of a type H generated with GenScatterHierarchy,
//     T is a type in the typelist used to generate H):
// Field<T>(obj)
// returns a reference to Unit<T>, where Unit is the template used to generate H 
////////////////////////////////////////////////////////////////////////////////

    template <class T, class H>
    typename H::template Rebind<T>::Result& Field(H& obj)
    {
        return obj;
    }
     
    template <class T, class H>
    const typename H::template Rebind<T>::Result& Field(const H& obj)
    {
        return obj;
    }
     
////////////////////////////////////////////////////////////////////////////////
// function template TupleUnit
// The building block of tuples 
////////////////////////////////////////////////////////////////////////////////

    template <class T>
    struct TupleUnit
    {
        T value_;
        operator T&() { return value_; }
        operator const T&() const { return value_; }
    };

////////////////////////////////////////////////////////////////////////////////
// class template Tuple
// Implements a tuple class that holds a number of values and provides field 
//     access to them via the Field function (below) 
////////////////////////////////////////////////////////////////////////////////

    template <class TList>
    struct Tuple : public GenScatterHierarchy<TList, TupleUnit>
    {
    };

////////////////////////////////////////////////////////////////////////////////
// helper class template FieldHelper
// See Field below
////////////////////////////////////////////////////////////////////////////////

    template <class H, unsigned int i> struct FieldHelper;
    
    template <class H>
    struct FieldHelper<H, 0>
    {
        typedef typename H::TList::Head ElementType;
        typedef typename H::template Rebind<ElementType>::Result UnitType;
        
        enum
        {
            isTuple = Conversion<UnitType, TupleUnit<ElementType> >::sameType,
            isConst = TypeTraits<H>::isConst
        };

        typedef const typename H::LeftBase ConstLeftBase;
        
        typedef typename Select<isConst, ConstLeftBase, 
            typename H::LeftBase>::Result LeftBase;
            
        typedef typename Select<isTuple, ElementType, 
            UnitType>::Result UnqualifiedResultType;

        typedef typename Select<isConst, const UnqualifiedResultType,
                		UnqualifiedResultType>::Result ResultType;
            
        static ResultType& Do(H& obj)
        {
            LeftBase& leftBase = obj;
            return leftBase;
        }
    };

    template <class H, unsigned int i>
    struct FieldHelper
    {
        typedef typename TL::TypeAt<typename H::TList, i>::Result ElementType;
        typedef typename H::template Rebind<ElementType>::Result UnitType;
        
        enum
        {
            isTuple = Conversion<UnitType, TupleUnit<ElementType> >::sameType,
            isConst = TypeTraits<H>::isConst
        };

        typedef const typename H::RightBase ConstRightBase;
        
        typedef typename Select<isConst, ConstRightBase, 
            typename H::RightBase>::Result RightBase;

        typedef typename Select<isTuple, ElementType, 
            UnitType>::Result UnqualifiedResultType;

        typedef typename Select<isConst, const UnqualifiedResultType,
                		UnqualifiedResultType>::Result ResultType;
            
        static ResultType& Do(H& obj)
        {
            RightBase& rightBase = obj;
            return FieldHelper<RightBase, i - 1>::Do(rightBase);
        }
    };

////////////////////////////////////////////////////////////////////////////////
// function template Field
// Accesses a field in an object of a type generated with GenScatterHierarchy
// Invocation (obj is an object of a type H generated with GenScatterHierarchy,
//     i is the index of a type in the typelist used to generate H):
// Field<i>(obj)
// returns a reference to Unit<T>, where Unit is the template used to generate H
//     and T is the i-th type in the typelist 
////////////////////////////////////////////////////////////////////////////////

    template <int i, class H>
    typename FieldHelper<H, i>::ResultType&
    Field(H& obj)
    {
        return FieldHelper<H, i>::Do(obj);
    }
        
//    template <int i, class H>
//    const typename FieldHelper<H, i>::ResultType&
//    Field(const H& obj)
//    {
//        return FieldHelper<H, i>::Do(obj);
//    }
        
////////////////////////////////////////////////////////////////////////////////
// class template GenLinearHierarchy
// Generates a linear hierarchy starting from a typelist and a template
// Invocation (TList is a typelist, Model is a template of two args):
// GenScatterHierarchy<TList, Model>
////////////////////////////////////////////////////////////////////////////////

    template
    <
        class TList,
        template <class AtomicType, class Base> class Unit,
        class Root = EmptyType
    >
    class GenLinearHierarchy;
    
    template
    <
        class T1,
        class T2,
        template <class, class> class Unit,
        class Root
    >
    class GenLinearHierarchy<Typelist<T1, T2>, Unit, Root>
        : public Unit< T1, GenLinearHierarchy<T2, Unit, Root> >
    {
    };

    template
    <
        class T,
        template <class, class> class Unit,
        class Root
    >
    class GenLinearHierarchy<Typelist<T, NullType>, Unit, Root>
        : public Unit<T, Root>
    {
    };

}   // namespace Loki

////////////////////////////////////////////////////////////////////////////////
// Change log:
// June 20, 2001: ported by Nick Thurn to gcc 2.95.3. Kudos, Nick!!!
// September 16, 2002: Fixed dependent template, using "::template" syntax. T.S.
////////////////////////////////////////////////////////////////////////////////

#endif // HIERARCHYGENERATORS_INC_
