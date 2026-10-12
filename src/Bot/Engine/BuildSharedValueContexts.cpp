#include "AiObjectContext.h"
#include "GDValueContext.h"  // DCGD1A: mod-playerbots #2733
#include "ValueContext.h"

void AiObjectContext::BuildSharedValueContexts(SharedNamedObjectContextList<UntypedValue>& valueContexts)
{
    valueContexts.Add(new ValueContext());
    valueContexts.Add(new WotlkDungeonGDValueContext());  // DCGD1A
}
