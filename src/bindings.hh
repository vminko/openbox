// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-
#ifndef __binding_hh
#define __binding_hh

/*! @file binding.hh
  @brief I dunno.. some binding stuff?
*/

#include "actions.hh"

#include <string>
#include <vector>

namespace ob {

class OBClient;

typedef struct Binding {
  unsigned int modifiers;
  unsigned int key;

  bool operator==(struct Binding &b2) { return key == b2.key &&
					  modifiers == b2.modifiers; }
  bool operator!=(struct Binding &b2) { return key != b2.key ||
					  modifiers != b2.modifiers; }
  Binding(unsigned int mod, unsigned int k) { modifiers = mod; key = k; }
} Binding;

typedef struct BindingTree {
  Binding binding;
  int id;     // the id given for the binding in add()
  bool chain; // true if this is a chain to another key (not an action)

  struct BindingTree *next_sibling; // the next binding in the tree at the same
                                    // level
  struct BindingTree *first_child;  // the first child of this binding (next
                                    // binding in a chained sequence).
  BindingTree(int id) : binding(0, 0) {
    this->id = id; chain = true; next_sibling = first_child = 0;
  }
  BindingTree() : binding(0, 0) {
    this->id = -1; chain = true; next_sibling = first_child = 0;
  }
} BindingTree;

class OBBindings {
public:
  //! A list of strings
  typedef std::vector<std::string> StringVect;

private:
  BindingTree _tree; // root node of the tree (this doesn't have siblings!)
  BindingTree *_curpos; // position in the keytree

  Binding _resetkey; // the key which resets the key chain status
  
  int find(BindingTree *search) const;
  bool translate(const std::string &str, Binding &b) const;
  BindingTree *buildtree(const StringVect &keylist, int id) const;
  void assimilate(BindingTree *node);

public:
  //! Initializes an OBBinding object
  OBBindings();
  //! Destroys the OBBinding object
  virtual ~OBBindings();

  //! Adds a new key binding
  /*!
    A binding will fail to be added if the binding already exists (as part of
    a chain or not), or if any of the strings in the keylist are invalid.    
    @return true if the binding could be added; false if it could not.
  */
  bool add(const StringVect &keylist, int id);

  //! Removes a key binding
  /*!
    @return The id of the binding that was removed, or '< 0' if none were
            removed.
  */
  int remove(const StringVect &keylist);

  //! Removes all key bindings
  void remove_all();

  //! Finds a keybinding and returns its id or '< 0' if it isn't found.
  /*!
    @return -1 if the keybinding was not found but does not conflict with
    any others; -2 if the keybinding conflicts with another.
  */
  int find(const StringVect &keylist);

  void fire(Window window, unsigned int modifiers,unsigned int key, Time time);

  void setResetKey(const std::string &key);

  void grabKeys(bool grab);
};

}

#endif // __binding_hh
