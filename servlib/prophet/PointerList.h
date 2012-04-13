/*
 *    Copyright 2006 Baylor University
 *
 *    Licensed under the Apache License, Version 2.0 (the "License");
 *    you may not use this file except in compliance with the License.
 *    You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 *    Unless required by applicable law or agreed to in writing, software
 *    distributed under the License is distributed on an "AS IS" BASIS,
 *    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *    See the License for the specific language governing permissions and
 *    limitations under the License.
 */

#ifndef _PROPHET_POINTER_LIST_H_
#define _PROPHET_POINTER_LIST_H_

#include <vector>

namespace prophet
{

/**
 * Auto deletes pointers in list destructor
 * This object assumes ownership for member pointers
 * Creates copies of members instead of copies of pointers to members
 */
template<class T>
class PointerList : public std::vector<T*>
{
public:
    typedef std::vector<T*> List;
    typedef typename std::vector<T*>::iterator iterator;
    typedef typename std::vector<T*>::const_iterator const_iterator;

    /**
     * Default constructor
     */
    PointerList()
        : std::vector<T*>() {}

    /**
     * Copy constructor
     */
    PointerList(const PointerList& a)
        : std::vector<T*>()
    {
        clear();
        copy_from(a);
    }

    /**
     * Destructor
     */
    virtual ~PointerList()
    {
        clear();
    }

    /**
     * Assignment operator creates deep copy, not pointer copy
     */
    PointerList& operator= (const PointerList& a)
    {
        clear();
        copy_from(a);
        return *this;
    }

    /**
     * Deletes member pointed to by iterator, then removes pointer
     */
    void erase(iterator i)
    {
        delete (*i);
        List::erase(i);
    }

    /**
     * Delete all member variables, then remove pointers from
     * container
     */
    void clear()
    {
        free();
        List::clear();
    }

protected:
    /**
     * Free memory pointed to by member variables
     */
    void free()
    {
        for(iterator i = List::begin();
            i != List::end();
            i++)
        {
            delete *i;
        }
    }
    /**
     * Utility function to perform deep copy from peer object
     */
    void copy_from(const PointerList& a)
    {
        for(const_iterator i = a.begin();
            i != a.end();
            i++)
        {
            this->push_back(new T(**i));
        }
    }
}; // template PointerList<T>

}; // namespace prophet

#endif // _PROPHET_POINTER_LIST_H_
