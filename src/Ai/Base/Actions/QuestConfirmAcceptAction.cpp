#include "QuestConfirmAcceptAction.h"

#include "WorldPacket.h"

bool QuestConfirmAcceptAction::Execute(Event event)
{
    WorldPacket packet(event.getPacket());
    uint32 questId;
    packet >> questId;

    WorldPacket sendPacket(CMSG_QUEST_CONFIRM_ACCEPT);
    sendPacket << questId;
    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
    if (!quest || !bot->CanAddQuest(quest, true))
    {
        return false;
    }
    std::ostringstream out;
    out << "Quest: " << chat->FormatQuest(quest) << " confirm accept";
    botAI->TellMaster(out);
    // 旧核心兼容：直接传原始 WorldPacket，不用新版类型化 QuestConfirmAcceptClient 包装
    bot->GetSession()->HandleQuestConfirmAccept(sendPacket);
    return true;
}
