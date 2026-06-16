#include "Channel.h"
#include "Reactor.h"

namespace Aether {
    void Channel::Remove(){ _loop->RemoveEvent(this); }
    void Channel::Update(){ _loop->UpdateEvent(this); }
}
