// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-
#ifndef __binding_hh
#define __binding_hh

/*! @file bindings.hh
  @brief I dunno.. some binding stuff?
*/

#include "actions.hh"
#include "python.hh"
#include "otk/timer.hh"

#include <string>
#include <list>
#include <vector>

namespace ob {

class Client;

struct MouseCallbackData {
  MouseCallback callback;
  void         *data;
  MouseCallbackData(MouseCallback c, void *d) : callback(c), data(d) {}
  void fire(MouseData *d) { callback(d, data); }
  bool operator==(const MouseCallbackData &other) { return (callback ==
                                                            other.callback &&
                                                            data ==
                                                            other.data); }
};

struct KeyCallbackData {
  KeyCallback callback;
  void       *data;
  KeyCallbackData(KeyCallback c, void *d) : callback(c), data(d) {}
  void fire(KeyData *d) { callback(d, data); }
  bool operator==(const KeyCallbackData &other) { return (callback ==
                                                          other.callback &&
                                                          data ==
                                                          other.data); }
};

struct EventCallbackData {
  EventCallback callback;
  void         *data;
  EventCallbackData(EventCallback c, void *d) : callback(c), data(d) {}
  void fire(EventData *d) { callback(d, data); }
  bool operator==(const EventCallbackData &other) { return (callback ==
                                                            other.callback &&
                                                            data ==
                                                            other.data); }
};

typedef std::list<MouseCallbackData> MouseCallbackList;
typedef std::list<KeyCallbackData> KeyCallbackList;
typedef std::list<EventCallbackData> EventCallbackList;

typedef struct Binding {
  unsigned int modifiers;
  unsigned int key;

  bool operator==(struct Binding &b2) { return key == b2.key &&
					  modifiers == b2.modifiers; }
  bool operator!=(struct Binding &b2) { return key != b2.key ||
					  modifiers != b2.modifiers; }
  Binding(unsigned int mod, unsigned int k) { modifiers = mod; key = k; }
} Binding;

typedef struct KeyBindingTree {
  Binding binding;
  KeyCallbackList callbacks; // the callbacks given for the binding in add()
  bool chain;     // true if this is a chain to another key (not an action)

  struct KeyBindingTree *next_sibling; // the next binding in the tree at the same
                                    // level
  struct KeyBindingTree *first_child;  // the first child of this binding (next
                                    // binding in a chained sequence).
  KeyBindingTree() : binding(0, 0) {
    chain = true; next_sibling = first_child = 0;
  }
} KeyBindingTree;

typedef struct ButtonBinding {
  Binding binding;
  MouseCallbackList callbacks[MouseAction::NUM_MOUSE_ACTION];
  ButtonBinding() : binding(0, 0) {}
};

class Bindings {
public:
  //! A list of strings
  typedef std::vector<std::string> StringVect;

private:
  // root node of the tree (this doesn't have siblings!)
  KeyBindingTree _keytree; 
  KeyBindingTree *_curpos; // position in the keytree

  Binding _resetkey; // the key which resets the key chain status

  otk::Timer *_timer;
  
  KeyBindingTree *find(KeyBindingTree *search, bool *conflict) const;
  KeyBindingTree *buildtree(const StringVect &keylist,
                            KeyCallback callback, void *data) const;
  void assimilate(KeyBindingTree *node);

  static void resetChains(Bindings *self); // the timer's timeout function

  typedef std::list <ButtonBinding*> ButtonBindingList;
  ButtonBindingList _buttons[MouseContext::NUM_MOUSE_CONTEXT];

  void grabButton(bool grab, const Binding &b, MouseContext::MC context,
                  Client *client);

  EventCallbackList _eventlist[EventAction::NUM_EVENT_ACTION];

  KeyCallbackData _keybgrab_callback;

  bool _grabbed;

public:
  //! Initializes an Bindings object
  Bindings();
  //! Destroys the Bindings object
  virtual ~Bindings();

  //! Translates a binding string into the actual Binding
  bool translate(const std::string &str, Binding &b, bool askey = true) const;
  
  //! Adds a new key binding
  /*!
    A binding will fail to be added if the binding already exists (as part of
    a chain or not), or if any of the strings in the keylist are invalid.    
    @return true if the binding could be added; false if it could not.
  */
  bool addKey(const StringVect &keylist, KeyCallback callback, void *data);

  ////! Removes a key binding
  ///*!
  //  @return The callbackid of the binding, or '< 0' if there was no binding to
  //          be removed.
  //*/
  //bool removeKey(const StringVect &keylist, KeyCallback callback, void *data);

  //! Removes all key bindings
  void removeAllKeys();

  void fireKey(int screen, unsigned int modifiers,unsigned int key, Time time,
               KeyAction::KA action);

  void setResetKey(const std::string &key);

  void grabKeys(bool grab);

  bool grabKeyboard(int screen, KeyCallback callback, void *data);
  void ungrabKeyboard();

  bool grabPointer(int screen);
  void ungrabPointer();

  bool addButton(const std::string &but, MouseContext::MC context,
                 MouseAction::MA action, MouseCallback callback, void *data);

  void grabButtons(bool grab, Client *client);

  //! Removes all button bindings
  void removeAllButtons();

  void fireButton(MouseData *data);

  //! Bind a callback for an event
  bool addEvent(EventAction::EA action, EventCallback callback, void *data);

  //! Unbind the callback function from an event
  bool removeEvent(EventAction::EA action, EventCallback callback, void *data);

  //! Remove all callback functions
  void removeAllEvents();

  void fireEvent(EventData *data);
};

}

#endif // __binding_hh
