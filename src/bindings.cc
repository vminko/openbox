// -*- mode: C++; indent-tabs-mode: nil; c-basic-offset: 2; -*-

#include "config.h"

#include "bindings.hh"
#include "screen.hh"
#include "openbox.hh"
#include "client.hh"
#include "frame.hh"
#include "otk/display.hh"

extern "C" {
#include <X11/Xlib.h>

#include "gettext.h"
#define _(str) gettext(str)
}

#include <algorithm>

namespace ob {

static bool buttonvalue(const std::string &button, unsigned int *val)
{
  if (button == "Left" || button == "1" || button == "Button1") {
    *val |= Button1;
  } else if (button == "Middle" || button == "2" || button == "Button2") {
    *val |= Button2;
  } else if (button == "Right" || button == "3" || button == "Button3") {
    *val |= Button3;
  } else if (button == "Up" || button == "4" || button == "Button4") {
    *val |= Button4;
  } else if (button == "Down" || button == "5" || button == "Button5") {
    *val |= Button5;
  } else
    return false;
  return true;
}

static bool modvalue(const std::string &mod, unsigned int *val)
{
  if (mod == "C") {           // control
    *val |= ControlMask;
  } else if (mod == "S") {    // shift
    *val |= ShiftMask;
  } else if (mod == "A" ||    // alt/mod1
             mod == "M" ||
             mod == "Mod1" ||
             mod == "M1") {
    *val |= Mod1Mask;
  } else if (mod == "Mod2" ||   // mod2
             mod == "M2") {
    *val |= Mod2Mask;
  } else if (mod == "Mod3" ||   // mod3
             mod == "M3") {
    *val |= Mod3Mask;
  } else if (mod == "W" ||    // windows/mod4
             mod == "Mod4" ||
             mod == "M4") {
    *val |= Mod4Mask;
  } else if (mod == "Mod5" ||   // mod5
             mod == "M5") {
    *val |= Mod5Mask;
  } else {                    // invalid
    return false;
  }
  return true;
}

bool Bindings::translate(const std::string &str, Binding &b,bool askey) const
{
  // parse out the base key name
  std::string::size_type keybegin = str.find_last_of('-');
  keybegin = (keybegin == std::string::npos) ? 0 : keybegin + 1;
  std::string key(str, keybegin);

  // parse out the requested modifier keys
  unsigned int modval = 0;
  std::string::size_type begin = 0, end;
  while (begin != keybegin) {
    end = str.find_first_of('-', begin);

    std::string mod(str, begin, end-begin);
    if (!modvalue(mod, &modval)) {
      printf(_("Invalid modifier element in key binding: %s\n"), mod.c_str());
      return false;
    }
    
    begin = end + 1;
  }

  // set the binding
  b.modifiers = modval;
  if (askey) {
    KeySym sym = XStringToKeysym(const_cast<char *>(key.c_str()));
    if (sym == NoSymbol) {
      printf(_("Invalid Key name in key binding: %s\n"), key.c_str());
      return false;
    }
    if (!(b.key = XKeysymToKeycode(**otk::display, sym)))
      printf(_("No valid keycode for Key in key binding: %s\n"), key.c_str());
    return b.key != 0;
  } else {
    return buttonvalue(key, &b.key);
  }
}

static void destroytree(KeyBindingTree *tree)
{
  while (tree) {
    KeyBindingTree *c = tree->first_child;
    delete tree;
    tree = c;
  }
}

KeyBindingTree *Bindings::buildtree(const StringVect &keylist,
                                    KeyCallback callback, void *data) const
{
  if (keylist.empty()) return 0; // nothing in the list.. return 0

  KeyBindingTree *ret = 0, *p;

  StringVect::const_reverse_iterator it, end = keylist.rend();
  for (it = keylist.rbegin(); it != end; ++it) {
    p = ret;
    ret = new KeyBindingTree();
    if (!p) {
      // this is the first built node, the bottom node of the tree
      ret->chain = false;
      ret->callbacks.push_back(KeyCallbackData(callback, data));
    }
    ret->first_child = p;
    if (!translate(*it, ret->binding)) {
      destroytree(ret);
      ret = 0;
      break;
    }
  }
  return ret;
}


Bindings::Bindings()
  : _curpos(&_keytree),
    _resetkey(0,0),
    _timer((otk::Timer *) 0),
    _keybgrab_callback(0, 0),
    _grabbed(0)
{
  setResetKey("C-g"); // set the default reset key
}


Bindings::~Bindings()
{
  if (_timer)
    delete _timer;
  if (_grabbed) {
    _grabbed = false;
    XUngrabKeyboard(**otk::display, CurrentTime);
  }
  removeAllKeys();
  //removeAllButtons(); // this is done by each client as they are unmanaged
  removeAllEvents();
}


void Bindings::assimilate(KeyBindingTree *node)
{
  KeyBindingTree *a, *b, *tmp, *last;

  if (!_keytree.first_child) {
    // there are no nodes at this level yet
    _keytree.first_child = node;
  } else {
    a = _keytree.first_child;
    last = a;
    b = node;
    while (a) {
      last = a;
      if (a->binding != b->binding) {
        a = a->next_sibling;
      } else {
        tmp = b;
        b = b->first_child;
        delete tmp;
        a = a->first_child;
      }
    }
    if (last->binding != b->binding)
      last->next_sibling = b;
    else {
      last->first_child = b->first_child;
      delete b;
    }
  }
}


KeyBindingTree *Bindings::find(KeyBindingTree *search,
                                 bool *conflict) const {
  *conflict = false;
  KeyBindingTree *a, *b;
  a = _keytree.first_child;
  b = search;
  while (a && b) {
    if (a->binding != b->binding) {
      a = a->next_sibling;
    } else {
      if (a->chain == b->chain) {
	if (!a->chain) {
          // found it! (return the actual id, not the search's)
	  return a;
        }
      } else {
        *conflict = true;
        return 0; // the chain status' don't match (conflict!)
      }
      b = b->first_child;
      a = a->first_child;
    }
  }
  return 0; // it just isn't in here
}


bool Bindings::addKey(const StringVect &keylist, KeyCallback callback,
                      void *data)
{
  KeyBindingTree *tree, *t;
  bool conflict;

  if (!(tree = buildtree(keylist, callback, data)))
    return false; // invalid binding requested

  t = find(tree, &conflict);
  if (conflict) {
    // conflicts with another binding
    destroytree(tree);
    return false;
  }

  if (t) {
    // already bound to something
    t->callbacks.push_back(KeyCallbackData(callback, data));
    destroytree(tree);
  } else {
    // grab the server here to make sure no key pressed go missed
    otk::display->grab();
    grabKeys(false);

    // assimilate this built tree into the main tree
    assimilate(tree); // assimilation destroys/uses the tree

    grabKeys(true); 
    otk::display->ungrab();
  }
 
  return true;
}

/*
bool Bindings::removeKey(const StringVect &keylist, KeyCallback callback, void *data)
{
  assert(false); // XXX: function not implemented yet

  KeyBindingTree *tree;
  bool conflict;

  if (!(tree = buildtree(keylist, 0)))
    return false; // invalid binding requested

  KeyBindingTree *t = find(tree, &conflict);
  if (t) {
    KeyCallbackList::iterator it = std::find(t->callbacks.begin(),
                                             t->callbacks.end(),
                                             callback);
    if (it != t->callbacks.end()) {
      // grab the server here to make sure no key pressed go missed
      otk::display->grab();
      grabKeys(false);
      
      _curpos = &_keytree;
      
      // XXX do shit here ...
      Py_XDECREF(*it);
      
      grabKeys(true);
      otk::display->ungrab();
      
      return true;
    }
  }
  return false;
}
*/

void Bindings::setResetKey(const std::string &key)
{
  Binding b(0, 0);
  if (translate(key, b)) {
    _resetkey.key = b.key;
    _resetkey.modifiers = b.modifiers;
  }
}


static void remove_branch(KeyBindingTree *first)
{
  KeyBindingTree *p = first;

  while (p) {
    if (p->first_child)
      remove_branch(p->first_child);
    KeyBindingTree *s = p->next_sibling;
    while(!p->callbacks.empty()) {
      p->callbacks.pop_front();
    }
    delete p;
    p = s;
  }
}


void Bindings::removeAllKeys()
{
  grabKeys(false);
  if (_keytree.first_child) {
    remove_branch(_keytree.first_child);
    _keytree.first_child = 0;
  }
  grabKeys(true);
}


void Bindings::grabKeys(bool grab)
{
  for (int i = 0; i < ScreenCount(**otk::display); ++i) {
    Screen *sc = openbox->screen(i);
    if (!sc) continue; // not a managed screen
    Window root = otk::display->screenInfo(i)->rootWindow();
    if (!grab) {
      otk::display->ungrabAllKeys(root);
      continue;
    }
    KeyBindingTree *p = _keytree.first_child;
    while (p) {
      otk::display->grabKey(p->binding.key, p->binding.modifiers,
                              root, false, GrabModeAsync, GrabModeSync,
                              false);
      p = p->next_sibling;
    }
  }
}


bool Bindings::grabKeyboard(int screen, KeyCallback callback, void *data)
{
  assert(callback);
  if (_keybgrab_callback.callback) return false; // already grabbed

  if (!openbox->screen(screen))
    return false; // the screen is not managed
  
  Window root = otk::display->screenInfo(screen)->rootWindow();
  if (XGrabKeyboard(**otk::display, root, false, GrabModeAsync,
                    GrabModeAsync, CurrentTime))
    return false;
  _keybgrab_callback.callback = callback;
  _keybgrab_callback.data = data;
  return true;
}


void Bindings::ungrabKeyboard()
{
  if (!_keybgrab_callback.callback) return; // not grabbed

  _keybgrab_callback = KeyCallbackData(0, 0);
  if (!_grabbed)  /* don't release out from under keychains */
    XUngrabKeyboard(**otk::display, CurrentTime);
  XUngrabPointer(**otk::display, CurrentTime);
}


bool Bindings::grabPointer(int screen)
{
  if (!openbox->screen(screen))
    return false; // the screen is not managed
  
  Window root = otk::display->screenInfo(screen)->rootWindow();
  XGrabPointer(**otk::display, root, false, 0, GrabModeAsync,
               GrabModeAsync, None, None, CurrentTime);
  return true;
}


void Bindings::ungrabPointer()
{
  XUngrabPointer(**otk::display, CurrentTime);
}


void Bindings::fireKey(int screen, unsigned int modifiers, unsigned int key,
                       Time time, KeyAction::KA action)
{
  if (_keybgrab_callback.callback) {
    Client *c = openbox->focusedClient();
    KeyData data(screen, c, time, modifiers, key, action);
    _keybgrab_callback.fire(&data);
  }

  // KeyRelease events only occur during keyboard grabs
  if (action == KeyAction::Release) return;
    
  if (key == _resetkey.key && modifiers == _resetkey.modifiers) {
    resetChains(this);
    XAllowEvents(**otk::display, AsyncKeyboard, CurrentTime);
  } else {
    KeyBindingTree *p = _curpos->first_child;
    while (p) {
      if (p->binding.key == key && p->binding.modifiers == modifiers) {
        if (p->chain) {
          if (_timer)
            delete _timer;
          _timer = new otk::Timer(5000, // 5 second timeout
                                  (otk::Timer::TimeoutHandler)resetChains,
                                  this);
          if (!_grabbed && !_keybgrab_callback.callback) {
            Window root = otk::display->screenInfo(screen)->rootWindow();
            //grab should never fail because we should have a sync grab at 
            //this point
            XGrabKeyboard(**otk::display, root, 0, GrabModeAsync, 
                          GrabModeSync, CurrentTime);
            _grabbed = true;
            _curpos = p;
          }
          XAllowEvents(**otk::display, AsyncKeyboard, CurrentTime);
        } else {
          Client *c = openbox->focusedClient();
          KeyData data(screen, c, time, modifiers, key, action);
          KeyCallbackList::iterator it, end = p->callbacks.end();
          for (it = p->callbacks.begin(); it != end; ++it)
            it->fire(&data);
          XAllowEvents(**otk::display, AsyncKeyboard, CurrentTime);
          resetChains(this);
        }
        break;
      }
      p = p->next_sibling;
    }
  }
}

void Bindings::resetChains(Bindings *self)
{
  if (self->_timer) {
    delete self->_timer;
    self->_timer = (otk::Timer *) 0;
  }
  self->_curpos = &self->_keytree;
  if (self->_grabbed) {
    self->_grabbed = false;
    if (!self->_keybgrab_callback.callback)
      XUngrabKeyboard(**otk::display, CurrentTime);
  }
}


bool Bindings::addButton(const std::string &but, MouseContext::MC context,
                         MouseAction::MA action, MouseCallback callback,
                         void *data)
{
  assert(context >= 0 && context < MouseContext::NUM_MOUSE_CONTEXT);
  assert(action >= 0 && action < MouseAction::NUM_MOUSE_ACTION);
  
  Binding b(0,0);
  if (!translate(but, b, false))
    return false;

  ButtonBindingList::iterator it, end = _buttons[context].end();

  // look for a duplicate binding
  for (it = _buttons[context].begin(); it != end; ++it)
    if ((*it)->binding.key == b.key &&
        (*it)->binding.modifiers == b.modifiers) {
      break;
    }

  ButtonBinding *bind;
  
  // the binding didnt exist yet, add it
  if (it == end) {
    bind = new ButtonBinding();
    bind->binding.key = b.key;
    bind->binding.modifiers = b.modifiers;
    _buttons[context].push_back(bind);
    // grab the button on all clients
    for (int sn = 0; sn < ScreenCount(**otk::display); ++sn) {
      Screen *s = openbox->screen(sn);
      if (!s) continue; // not managed
      Client::List::iterator c_it, c_end = s->clients.end();
      for (c_it = s->clients.begin(); c_it != c_end; ++c_it) {
        grabButton(true, bind->binding, context, *c_it);
      }
    }
  } else
    bind = *it;
  bind->callbacks[action].push_back(MouseCallbackData(callback, data));
  return true;
}

void Bindings::removeAllButtons()
{
  for (int i = 0; i < MouseContext::NUM_MOUSE_CONTEXT; ++i) {
    ButtonBindingList::iterator it, end = _buttons[i].end();
    for (it = _buttons[i].begin(); it != end; ++it) {
      for (int a = 0; a < MouseAction::NUM_MOUSE_ACTION; ++a) {
        while (!(*it)->callbacks[a].empty()) {
          (*it)->callbacks[a].pop_front();
        }
      }
      // ungrab the button on all clients
      for (int sn = 0; sn < ScreenCount(**otk::display); ++sn) {
        Screen *s = openbox->screen(sn);
        if (!s) continue; // not managed
        Client::List::iterator c_it, c_end = s->clients.end();
        for (c_it = s->clients.begin(); c_it != c_end; ++c_it) {
          grabButton(false, (*it)->binding, (MouseContext::MC)i, *c_it);
        }
      }
    }
  }
}

void Bindings::grabButton(bool grab, const Binding &b,
                          MouseContext::MC context, Client *client)
{
  Window win;
  int mode = GrabModeAsync;
  unsigned int mask;
  switch(context) {
  case MouseContext::Frame:
    win = client->frame->window();
    mask = ButtonPressMask | ButtonMotionMask | ButtonReleaseMask;
    break;
  case MouseContext::Window:
    win = client->frame->plate();
    mode = GrabModeSync; // this is handled in fireButton
    mask = ButtonPressMask; // can't catch more than this with Sync mode
                            // the release event is manufactured by the
                            // master buttonPressHandler
    break;
  default:
    // any other elements already get button events, don't grab on them
    return;
  }
  if (grab)
    otk::display->grabButton(b.key, b.modifiers, win, false, mask, mode,
                             GrabModeAsync, None, None, false);
  else
    otk::display->ungrabButton(b.key, b.modifiers, win);
}

void Bindings::grabButtons(bool grab, Client *client)
{
  for (int i = 0; i < MouseContext::NUM_MOUSE_CONTEXT; ++i) {
    ButtonBindingList::iterator it, end = _buttons[i].end();
    for (it = _buttons[i].begin(); it != end; ++it)
      grabButton(grab, (*it)->binding, (MouseContext::MC)i, client);
  }
}

void Bindings::fireButton(MouseData *data)
{
  if (data->context == MouseContext::Window) {
    // Replay the event, so it goes to the client
    XAllowEvents(**otk::display, ReplayPointer, data->time);
  }
  
  ButtonBindingList::iterator it, end = _buttons[data->context].end();
  for (it = _buttons[data->context].begin(); it != end; ++it)
    if ((*it)->binding.key == data->button &&
        (*it)->binding.modifiers == data->state) {
      MouseCallbackList::iterator c_it,
        c_end = (*it)->callbacks[data->action].end();
      for (c_it = (*it)->callbacks[data->action].begin();
           c_it != c_end; ++c_it)
        c_it->fire(data);
    }
}


bool Bindings::addEvent(EventAction::EA action, EventCallback callback,
                        void *data)
{
  if (action < 0 || action >= EventAction::NUM_EVENT_ACTION) {
    return false;
  }
#ifdef    XKB
  if (action == EventAction::Bell && _eventlist[action].empty())
    XkbSelectEvents(**otk::display, XkbUseCoreKbd,
                    XkbBellNotifyMask, XkbBellNotifyMask);
#endif // XKB
  _eventlist[action].push_back(EventCallbackData(callback, data));
  return true;
}

bool Bindings::removeEvent(EventAction::EA action, EventCallback callback,
                           void *data)
{
  if (action < 0 || action >= EventAction::NUM_EVENT_ACTION) {
    return false;
  }
  
  EventCallbackList::iterator it = std::find(_eventlist[action].begin(),
                                             _eventlist[action].end(),
                                             EventCallbackData(callback,
                                                               data));
  if (it != _eventlist[action].end()) {
    _eventlist[action].erase(it);
#ifdef    XKB
    if (action == EventAction::Bell && _eventlist[action].empty())
      XkbSelectEvents(**otk::display, XkbUseCoreKbd,
                      XkbBellNotifyMask, 0);
#endif // XKB
    return true;
  }
  return false;
}

void Bindings::removeAllEvents()
{
  for (int i = 0; i < EventAction::NUM_EVENT_ACTION; ++i) {
    while (!_eventlist[i].empty()) {
      _eventlist[i].pop_front();
    }
  }
}

void Bindings::fireEvent(EventData *data)
{
  EventCallbackList::iterator c_it, c_end = _eventlist[data->action].end();
  for (c_it = _eventlist[data->action].begin(); c_it != c_end; ++c_it)
    c_it->fire(data);
}

}
