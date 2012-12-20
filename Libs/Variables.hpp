/*
=================================================================================
This file is part of Cafu, the open-source game engine and graphics engine
for multiplayer, cross-platform, real-time 3D action.
Copyright (C) 2002-2012 Carsten Fuchs Software.

Cafu is free software: you can redistribute it and/or modify it under the terms
of the GNU General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.

Cafu is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Cafu. If not, see <http://www.gnu.org/licenses/>.

For support and more information about Cafu, visit us at <http://www.cafu.de>.
=================================================================================
*/

#ifndef CAFU_TYPESYS_VARIABLES_HPP_INCLUDED
#define CAFU_TYPESYS_VARIABLES_HPP_INCLUDED

#include "Templates/Array.hpp"
#include "Math3D/Vector3.hpp"

#include <cstring>
#include <map>


/**
@page OverviewVariables TypeSys Variables Overview

The "variable" classes in cf::TypeSys are supposed to turn a "normal" member
variable of a class into something "more" that also other code can work with.

Problem and Motivation
======================

In Cafu we use "components" to compose map entities and GUI windows, see the
cf::GuiSys::ComponentBaseT hierarchy for an example.
Components are classes like any other, but their member variables should be:

  - editable in our graphical map and GUI editor CaWE,
  - accessible via scripting, and
  - easy to serialize and deserialize (maps, GUIs, prefabs).

However, none of these features should impact the component in its actual,
inherent purpose, being a component. The cf::GuiSys::ComponentBaseT classes
should not be hindered in their performance, and their interfaces and
implementations should not be cluttered with the details of the above
requirements.
In fact, it would be desireable to separate these issues from the components,
so that the way in which the editors let users edit the variables or the
details in which variables are bound to scripts can be varied without ever
affecting the components themselves.

How it works
============

Consider the typical code structure of a component that has a member variable `x`:

~~~~~~~~~~~~~~~~{cpp}
class SomeComponentT
{
    public:

    // [ other methods ... ]
    void SetX(int newX);
    int GetX() const;


    private:

    int x;
};
~~~~~~~~~~~~~~~~

A member variable usually implies three items: the variable itself, a "Get"
function and a "Set" function.
The implementations of the two functions are often trivial and `inline`, but
it is not unusual that especially the "Set" function has side-effects, such as
updating a graphical resource if a new filename has been set, etc.

Now, the key idea is to replace the variable and its Get/Set functions with a
"wrapper" class.
This is what the cf::TypeSys::VarT classes in this file are for.

As we also need a list of all cf::TypeSys::VarT instances in the class that
contains them (the component), the cf::TypeSys::VarT classes all must derive
from a common base class, cf::TypeSys::VarBaseT.
As the list of variables should actually have more features than a plain array
or list, we manage them in a cf::TypeSys::VarManT instance.

Finally, we apply the [Visitor pattern](http://en.wikipedia.org/wiki/Visitor_pattern)
to the cf::TypeSys::VarBaseT hierarchy: It allows user code to add arbitrary
operations to the cf::TypeSys::VarBaseT instances without modifying them.
*/


namespace cf
{
    namespace TypeSys
    {
        class VisitorT;
        class VisitorConstT;


        /// This is the common base class for the VarT classes.
        ///
        /// It allows other code to work with VarT%s without having to know their concrete type.
        /// For example, VarManT is a container for pointers to VarBaseT%s.
        ///
        /// @see \ref OverviewVariables
        class VarBaseT
        {
            public:

            VarBaseT(const char* Name, const char* Flags[])
                : m_Name(Name), m_Flags(Flags) { }

            const char* GetName() const { return m_Name; }

            const char** GetFlags() const { return m_Flags; }
            bool HasFlag(const char* Flag) const;

            virtual void accept(VisitorT&      Visitor) = 0;
            virtual void accept(VisitorConstT& Visitor) const = 0;


            private:

            const char*  m_Name;    ///< The name of the variable.
            const char** m_Flags;   ///< An optional list of context-dependent flags.
        };


        /// This is a "wrapper" around a normal C++ variable.
        ///
        /// It can be used in place of a normal variable whenever the functionality
        /// described in \ref OverviewVariables is desired, for example in the member
        /// variables of component classes of game entities and GUI windows.
        ///
        /// User code can derive from this class and override the Set() method in
        /// order to customize the behaviour.
        ///
        /// @see \ref OverviewVariables
        template<class T> class VarT : public VarBaseT
        {
            public:

            /// The constructor.
            VarT(const char* Name, const T& Value, const char* Flags[]=NULL)
                : VarBaseT(Name, Flags), m_Value(Value) { }

            /// Returns the value of this variable.
            const T& Get() const { return m_Value; }

            /// Sets the value of this variable to the given value `v`.
            virtual void Set(const T& v) { m_Value = v; }

            void accept(VisitorT&      Visitor);
            void accept(VisitorConstT& Visitor) const;


            private:

            T m_Value;  ///< The actual variable that is wrapped by this class.
        };


        /// This is the base class for the visitors of VarT%s.
        ///
        /// With the Visitor pattern, the data structure being used is independent
        /// of the uses to which it is being put.
        ///
        /// @see \ref OverviewVariables
        class VisitorT
        {
            public:

            virtual ~VisitorT() { }

            virtual void visit(VarT<float>& Var) = 0;
            virtual void visit(VarT<double>& Var) = 0;
            virtual void visit(VarT<int>& Var) = 0;
            virtual void visit(VarT<std::string>& Var) = 0;
            virtual void visit(VarT<Vector3fT>& Var) = 0;
        };


        /// Like VisitorT, but for `const` VarT%s.
        ///
        /// @see \ref OverviewVariables.
        class VisitorConstT
        {
            public:

            virtual ~VisitorConstT() { }

            virtual void visit(const VarT<float>& Var) = 0;
            virtual void visit(const VarT<double>& Var) = 0;
            virtual void visit(const VarT<int>& Var) = 0;
            virtual void visit(const VarT<std::string>& Var) = 0;
            virtual void visit(const VarT<Vector3fT>& Var) = 0;
        };


        /// This class is a simple container for pointers to VarBaseTs.
        ///
        /// Together with the VarT classes, it provides a very simple kind of
        /// "reflection" or "type introspection" feature.
        /// See class ComponentBaseT for an example use.
        ///
        /// @see \ref OverviewVariables
        class VarManT
        {
            public:

            struct CompareCStr
            {
                // See "Die C++ Programmiersprache" by Bjarne Stroustrup pages 498 and 510 and
                // Scott Meyers "Effective STL" Item 21 for more information about this struct.
                bool operator () (const char* a, const char* b) const { return std::strcmp(a, b) < 0; }
            };

            typedef std::map<const char*, VarBaseT*, CompareCStr> MapVarBaseT;


            void Add(VarBaseT* Var);

            const ArrayT<VarBaseT*>& GetArray() const { return m_VarsArray; }

            const MapVarBaseT& GetMap() const { return m_VarsMap; }

            VarBaseT* Find(const char* Name) const
            {
                const MapVarBaseT::const_iterator It = m_VarsMap.find(Name);

                return It != m_VarsMap.end() ? It->second : NULL;
            }


            private:

            ArrayT<VarBaseT*> m_VarsArray;  ///< Keeps the variables in the order they were added.
            MapVarBaseT       m_VarsMap;    ///< Keeps the variables by name (for find by name and lexicographical traversal).
        };
    }
}

#endif
