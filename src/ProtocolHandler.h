/******************************************************************************
    Copyright (C) 2002-2022 Heroes of Argentum Developers

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/


#ifndef PROTOCOLHANDLER_H_
#define PROTOCOLHANDLER_H_

#include "ProtocolNew.h"


void HandleIncomingData(int UserIndex);

void HandleIncomingDataOnePacket(int UserIndex);

class HoAClientPacketHandler : public hoa::protocol::client::ClientPacketHandler, public hoa::protocol::clientgm::ClientGMPacketHandler {
public:
	HoAClientPacketHandler(int UserIndex_) : UserIndex(UserIndex_) {}

    virtual ~HoAClientPacketHandler() {}

    virtual void handleLoginExistingChar(hoa::protocol::client::LoginExistingChar* p);
    virtual void handleThrowDices();
    virtual void handleTalk(hoa::protocol::client::Talk* p);
    virtual void handleYell(hoa::protocol::client::Yell* p);
    virtual void handleWhisper(hoa::protocol::client::Whisper* p);
    virtual void handleWalk(hoa::protocol::client::Walk* p);
    virtual void handleRequestPositionUpdate(hoa::protocol::client::RequestPositionUpdate* p);
    virtual void handleAttack(hoa::protocol::client::Attack* p);
    virtual void handlePickUp(hoa::protocol::client::PickUp* p);
    virtual void handleSafeToggle(hoa::protocol::client::SafeToggle* p);
    virtual void handleResuscitationSafeToggle(hoa::protocol::client::ResuscitationSafeToggle* p);
    virtual void handleRequestGuildLeaderInfo(hoa::protocol::client::RequestGuildLeaderInfo* p);
    virtual void handleRequestAtributes(hoa::protocol::client::RequestAtributes* p);
    virtual void handleRequestFame(hoa::protocol::client::RequestFame* p);
    virtual void handleRequestSkills(hoa::protocol::client::RequestSkills* p);
    virtual void handleRequestMiniStats(hoa::protocol::client::RequestMiniStats* p);
    virtual void handleCommerceEnd(hoa::protocol::client::CommerceEnd* p);
    virtual void handleUserCommerceEnd(hoa::protocol::client::UserCommerceEnd* p);
    virtual void handleUserCommerceConfirm(hoa::protocol::client::UserCommerceConfirm* p);
    virtual void handleCommerceChat(hoa::protocol::client::CommerceChat* p);
    virtual void handleBankEnd(hoa::protocol::client::BankEnd* p);
    virtual void handleUserCommerceOk(hoa::protocol::client::UserCommerceOk* p);
    virtual void handleUserCommerceReject(hoa::protocol::client::UserCommerceReject* p);
    virtual void handleDrop(hoa::protocol::client::Drop* p);
    virtual void handleCastSpell(hoa::protocol::client::CastSpell* p);
    virtual void handleLeftClick(hoa::protocol::client::LeftClick* p);
    virtual void handleDoubleClick(hoa::protocol::client::DoubleClick* p);
    virtual void handleWork(hoa::protocol::client::Work* p);
    virtual void handleUseSpellMacro(hoa::protocol::client::UseSpellMacro* p);
    virtual void handleUseItem(hoa::protocol::client::UseItem* p);
    virtual void handleCraftBlacksmith(hoa::protocol::client::CraftBlacksmith* p);
    virtual void handleCraftCarpenter(hoa::protocol::client::CraftCarpenter* p);
    virtual void handleWorkLeftClick(hoa::protocol::client::WorkLeftClick* p);
    virtual void handleCreateNewGuild(hoa::protocol::client::CreateNewGuild* p);
    virtual void handleSpellInfo(hoa::protocol::client::SpellInfo* p);
    virtual void handleEquipItem(hoa::protocol::client::EquipItem* p);
    virtual void handleChangeHeading(hoa::protocol::client::ChangeHeading* p);
    virtual void handleModifySkills(hoa::protocol::client::ModifySkills* p);
    virtual void handleTrain(hoa::protocol::client::Train* p);
    virtual void handleCommerceBuy(hoa::protocol::client::CommerceBuy* p);
    virtual void handleBankExtractItem(hoa::protocol::client::BankExtractItem* p);
    virtual void handleCommerceSell(hoa::protocol::client::CommerceSell* p);
    virtual void handleBankDeposit(hoa::protocol::client::BankDeposit* p);
    virtual void handleForumPost(hoa::protocol::client::ForumPost* p);
    virtual void handleMoveSpell(hoa::protocol::client::MoveSpell* p);
    virtual void handleMoveBank(hoa::protocol::client::MoveBank* p);
    virtual void handleClanCodexUpdate(hoa::protocol::client::ClanCodexUpdate* p);
    virtual void handleUserCommerceOffer(hoa::protocol::client::UserCommerceOffer* p);
    virtual void handleGuildAcceptPeace(hoa::protocol::client::GuildAcceptPeace* p);
    virtual void handleGuildRejectAlliance(hoa::protocol::client::GuildRejectAlliance* p);
    virtual void handleGuildRejectPeace(hoa::protocol::client::GuildRejectPeace* p);
    virtual void handleGuildAcceptAlliance(hoa::protocol::client::GuildAcceptAlliance* p);
    virtual void handleGuildOfferPeace(hoa::protocol::client::GuildOfferPeace* p);
    virtual void handleGuildOfferAlliance(hoa::protocol::client::GuildOfferAlliance* p);
    virtual void handleGuildAllianceDetails(hoa::protocol::client::GuildAllianceDetails* p);
    virtual void handleGuildPeaceDetails(hoa::protocol::client::GuildPeaceDetails* p);
    virtual void handleGuildRequestJoinerInfo(hoa::protocol::client::GuildRequestJoinerInfo* p);
    virtual void handleGuildAlliancePropList(hoa::protocol::client::GuildAlliancePropList* p);
    virtual void handleGuildPeacePropList(hoa::protocol::client::GuildPeacePropList* p);
    virtual void handleGuildDeclareWar(hoa::protocol::client::GuildDeclareWar* p);
    virtual void handleGuildNewWebsite(hoa::protocol::client::GuildNewWebsite* p);
    virtual void handleGuildAcceptNewMember(hoa::protocol::client::GuildAcceptNewMember* p);
    virtual void handleGuildRejectNewMember(hoa::protocol::client::GuildRejectNewMember* p);
    virtual void handleGuildKickMember(hoa::protocol::client::GuildKickMember* p);
    virtual void handleGuildUpdateNews(hoa::protocol::client::GuildUpdateNews* p);
    virtual void handleGuildMemberInfo(hoa::protocol::client::GuildMemberInfo* p);
    virtual void handleGuildOpenElections(hoa::protocol::client::GuildOpenElections* p);
    virtual void handleGuildRequestMembership(hoa::protocol::client::GuildRequestMembership* p);
    virtual void handleGuildRequestDetails(hoa::protocol::client::GuildRequestDetails* p);
    virtual void handleOnline(hoa::protocol::client::Online* p);
    virtual void handleQuit(hoa::protocol::client::Quit* p);
    virtual void handleGuildLeave(hoa::protocol::client::GuildLeave* p);
    virtual void handleRequestAccountState(hoa::protocol::client::RequestAccountState* p);
    virtual void handlePetStand(hoa::protocol::client::PetStand* p);
    virtual void handlePetFollow(hoa::protocol::client::PetFollow* p);
    virtual void handleReleasePet(hoa::protocol::client::ReleasePet* p);
    virtual void handleTrainList(hoa::protocol::client::TrainList* p);
    virtual void handleRest(hoa::protocol::client::Rest* p);
    virtual void handleMeditate(hoa::protocol::client::Meditate* p);
    virtual void handleResucitate(hoa::protocol::client::Resucitate* p);
    virtual void handleHeal(hoa::protocol::client::Heal* p);
    virtual void handleHelp(hoa::protocol::client::Help* p);
    virtual void handleRequestStats(hoa::protocol::client::RequestStats* p);
    virtual void handleCommerceStart(hoa::protocol::client::CommerceStart* p);
    virtual void handleBankStart(hoa::protocol::client::BankStart* p);
    virtual void handleEnlist(hoa::protocol::client::Enlist* p);
    virtual void handleInformation(hoa::protocol::client::Information* p);
    virtual void handleReward(hoa::protocol::client::Reward* p);
    virtual void handleRequestMOTD(hoa::protocol::client::RequestMOTD* p);
    virtual void handleUpTime(hoa::protocol::client::UpTime* p);
    virtual void handlePartyLeave(hoa::protocol::client::PartyLeave* p);
    virtual void handlePartyCreate(hoa::protocol::client::PartyCreate* p);
    virtual void handlePartyJoin(hoa::protocol::client::PartyJoin* p);
    virtual void handleInquiry(hoa::protocol::client::Inquiry* p);
    virtual void handleGuildMessage(hoa::protocol::client::GuildMessage* p);
    virtual void handlePartyMessage(hoa::protocol::client::PartyMessage* p);
    virtual void handleCentinelReport(hoa::protocol::client::CentinelReport* p);
    virtual void handleGuildOnline(hoa::protocol::client::GuildOnline* p);
    virtual void handlePartyOnline(hoa::protocol::client::PartyOnline* p);
    virtual void handleCouncilMessage(hoa::protocol::client::CouncilMessage* p);
    virtual void handleRoleMasterRequest(hoa::protocol::client::RoleMasterRequest* p);
    virtual void handleGMRequest(hoa::protocol::client::GMRequest* p);
    virtual void handleBugReport(hoa::protocol::client::BugReport* p);
    virtual void handleChangeDescription(hoa::protocol::client::ChangeDescription* p);
    virtual void handleGuildVote(hoa::protocol::client::GuildVote* p);
    virtual void handlePunishments(hoa::protocol::client::Punishments* p);
    virtual void handleGamble(hoa::protocol::client::Gamble* p);
    virtual void handleInquiryVote(hoa::protocol::client::InquiryVote* p);
    virtual void handleLeaveFaction(hoa::protocol::client::LeaveFaction* p);
    virtual void handleBankExtractGold(hoa::protocol::client::BankExtractGold* p);
    virtual void handleBankDepositGold(hoa::protocol::client::BankDepositGold* p);
    virtual void handleDenounce(hoa::protocol::client::Denounce* p);
    virtual void handleGuildFundate(hoa::protocol::client::GuildFundate* p);
    virtual void handleGuildFundation(hoa::protocol::client::GuildFundation* p);
    virtual void handlePartyKick(hoa::protocol::client::PartyKick* p);
    virtual void handlePartySetLeader(hoa::protocol::client::PartySetLeader* p);
    virtual void handlePartyAcceptMember(hoa::protocol::client::PartyAcceptMember* p);
    virtual void handlePing(hoa::protocol::client::Ping* p);
    virtual void handleRequestPartyForm(hoa::protocol::client::RequestPartyForm* p);
    virtual void handleItemUpgrade(hoa::protocol::client::ItemUpgrade* p);
    virtual void handleGMCommands(hoa::protocol::client::GMCommands* p);
    virtual void handleInitCrafting(hoa::protocol::client::InitCrafting* p);
    virtual void handleHome(hoa::protocol::client::Home* p);
    virtual void handleShowGuildNews(hoa::protocol::client::ShowGuildNews* p);
    virtual void handleShareNpc(hoa::protocol::client::ShareNpc* p);
    virtual void handleStopSharingNpc(hoa::protocol::client::StopSharingNpc* p);
    virtual void handleConsultation(hoa::protocol::client::Consultation* p);
    virtual void handleMoveItem(hoa::protocol::client::MoveItem* p);

public:

    virtual void handleGMMessage(hoa::protocol::clientgm::GMMessage* p);
    virtual void handleShowName(hoa::protocol::clientgm::ShowName* p);
    virtual void handleOnlineRoyalArmy(hoa::protocol::clientgm::OnlineRoyalArmy* p);
    virtual void handleOnlineChaosLegion(hoa::protocol::clientgm::OnlineChaosLegion* p);
    virtual void handleGoNearby(hoa::protocol::clientgm::GoNearby* p);
    virtual void handleComment(hoa::protocol::clientgm::Comment* p);
    virtual void handleServerTime(hoa::protocol::clientgm::ServerTime* p);
    virtual void handleWhere(hoa::protocol::clientgm::Where* p);
    virtual void handleCreaturesInMap(hoa::protocol::clientgm::CreaturesInMap* p);
    virtual void handleWarpMeToTarget(hoa::protocol::clientgm::WarpMeToTarget* p);
    virtual void handleWarpChar(hoa::protocol::clientgm::WarpChar* p);
    virtual void handleSilence(hoa::protocol::clientgm::Silence* p);
    virtual void handleSOSShowList(hoa::protocol::clientgm::SOSShowList* p);
    virtual void handleSOSRemove(hoa::protocol::clientgm::SOSRemove* p);
    virtual void handleGoToChar(hoa::protocol::clientgm::GoToChar* p);
    virtual void handleInvisible(hoa::protocol::clientgm::Invisible* p);
    virtual void handleGMPanel(hoa::protocol::clientgm::GMPanel* p);
    virtual void handleRequestUserList(hoa::protocol::clientgm::RequestUserList* p);
    virtual void handleWorking(hoa::protocol::clientgm::Working* p);
    virtual void handleHiding(hoa::protocol::clientgm::Hiding* p);
    virtual void handleJail(hoa::protocol::clientgm::Jail* p);
    virtual void handleKillNPC(hoa::protocol::clientgm::KillNPC* p);
    virtual void handleWarnUser(hoa::protocol::clientgm::WarnUser* p);
    virtual void handleEditChar(hoa::protocol::clientgm::EditChar* p);
    virtual void handleRequestCharInfo(hoa::protocol::clientgm::RequestCharInfo* p);
    virtual void handleRequestCharStats(hoa::protocol::clientgm::RequestCharStats* p);
    virtual void handleRequestCharGold(hoa::protocol::clientgm::RequestCharGold* p);
    virtual void handleRequestCharInventory(hoa::protocol::clientgm::RequestCharInventory* p);
    virtual void handleRequestCharBank(hoa::protocol::clientgm::RequestCharBank* p);
    virtual void handleRequestCharSkills(hoa::protocol::clientgm::RequestCharSkills* p);
    virtual void handleReviveChar(hoa::protocol::clientgm::ReviveChar* p);
    virtual void handleOnlineGM(hoa::protocol::clientgm::OnlineGM* p);
    virtual void handleOnlineMap(hoa::protocol::clientgm::OnlineMap* p);
    virtual void handleForgive(hoa::protocol::clientgm::Forgive* p);
    virtual void handleKick(hoa::protocol::clientgm::Kick* p);
    virtual void handleExecute(hoa::protocol::clientgm::Execute* p);
    virtual void handleBanChar(hoa::protocol::clientgm::BanChar* p);
    virtual void handleUnbanChar(hoa::protocol::clientgm::UnbanChar* p);
    virtual void handleNPCFollow(hoa::protocol::clientgm::NPCFollow* p);
    virtual void handleSummonChar(hoa::protocol::clientgm::SummonChar* p);
    virtual void handleSpawnListRequest(hoa::protocol::clientgm::SpawnListRequest* p);
    virtual void handleSpawnCreature(hoa::protocol::clientgm::SpawnCreature* p);
    virtual void handleResetNPCInventory(hoa::protocol::clientgm::ResetNPCInventory* p);
    virtual void handleCleanWorld(hoa::protocol::clientgm::CleanWorld* p);
    virtual void handleServerMessage(hoa::protocol::clientgm::ServerMessage* p);
    virtual void handleNickToIP(hoa::protocol::clientgm::NickToIP* p);
    virtual void handleIPToNick(hoa::protocol::clientgm::IPToNick* p);
    virtual void handleGuildOnlineMembers(hoa::protocol::clientgm::GuildOnlineMembers* p);
    virtual void handleTeleportCreate(hoa::protocol::clientgm::TeleportCreate* p);
    virtual void handleTeleportDestroy(hoa::protocol::clientgm::TeleportDestroy* p);
    virtual void handleRainToggle(hoa::protocol::clientgm::RainToggle* p);
    virtual void handleSetCharDescription(hoa::protocol::clientgm::SetCharDescription* p);
    virtual void handleForceMIDIToMap(hoa::protocol::clientgm::ForceMIDIToMap* p);
    virtual void handleForceWAVEToMap(hoa::protocol::clientgm::ForceWAVEToMap* p);
    virtual void handleRoyalArmyMessage(hoa::protocol::clientgm::RoyalArmyMessage* p);
    virtual void handleChaosLegionMessage(hoa::protocol::clientgm::ChaosLegionMessage* p);
    virtual void handleCitizenMessage(hoa::protocol::clientgm::CitizenMessage* p);
    virtual void handleCriminalMessage(hoa::protocol::clientgm::CriminalMessage* p);
    virtual void handleTalkAsNPC(hoa::protocol::clientgm::TalkAsNPC* p);
    virtual void handleDestroyAllItemsInArea(hoa::protocol::clientgm::DestroyAllItemsInArea* p);
    virtual void handleAcceptRoyalCouncilMember(hoa::protocol::clientgm::AcceptRoyalCouncilMember* p);
    virtual void handleAcceptChaosCouncilMember(hoa::protocol::clientgm::AcceptChaosCouncilMember* p);
    virtual void handleItemsInTheFloor(hoa::protocol::clientgm::ItemsInTheFloor* p);
    virtual void handleMakeDumb(hoa::protocol::clientgm::MakeDumb* p);
    virtual void handleMakeDumbNoMore(hoa::protocol::clientgm::MakeDumbNoMore* p);
    virtual void handleDumpIPTables(hoa::protocol::clientgm::DumpIPTables* p);
    virtual void handleCouncilKick(hoa::protocol::clientgm::CouncilKick* p);
    virtual void handleSetTrigger(hoa::protocol::clientgm::SetTrigger* p);
    virtual void handleAskTrigger(hoa::protocol::clientgm::AskTrigger* p);
    virtual void handleBannedIPList(hoa::protocol::clientgm::BannedIPList* p);
    virtual void handleBannedIPReload(hoa::protocol::clientgm::BannedIPReload* p);
    virtual void handleGuildMemberList(hoa::protocol::clientgm::GuildMemberList* p);
    virtual void handleGuildBan(hoa::protocol::clientgm::GuildBan* p);
    virtual void handleBanIP(hoa::protocol::clientgm::BanIP* p);
    virtual void handleUnbanIP(hoa::protocol::clientgm::UnbanIP* p);
    virtual void handleCreateItem(hoa::protocol::clientgm::CreateItem* p);
    virtual void handleDestroyItems(hoa::protocol::clientgm::DestroyItems* p);
    virtual void handleChaosLegionKick(hoa::protocol::clientgm::ChaosLegionKick* p);
    virtual void handleRoyalArmyKick(hoa::protocol::clientgm::RoyalArmyKick* p);
    virtual void handleForceMIDIAll(hoa::protocol::clientgm::ForceMIDIAll* p);
    virtual void handleForceWAVEAll(hoa::protocol::clientgm::ForceWAVEAll* p);
    virtual void handleRemovePunishment(hoa::protocol::clientgm::RemovePunishment* p);
    virtual void handleTileBlockedToggle(hoa::protocol::clientgm::TileBlockedToggle* p);
    virtual void handleKillNPCNoRespawn(hoa::protocol::clientgm::KillNPCNoRespawn* p);
    virtual void handleKillAllNearbyNPCs(hoa::protocol::clientgm::KillAllNearbyNPCs* p);
    virtual void handleLastIP(hoa::protocol::clientgm::LastIP* p);
    virtual void handleChangeMOTD(hoa::protocol::clientgm::ChangeMOTD* p);
    virtual void handleSetMOTD(hoa::protocol::clientgm::SetMOTD* p);
    virtual void handleSystemMessage(hoa::protocol::clientgm::SystemMessage* p);
    virtual void handleCreateNPC(hoa::protocol::clientgm::CreateNPC* p);
    virtual void handleCreateNPCWithRespawn(hoa::protocol::clientgm::CreateNPCWithRespawn* p);
    virtual void handleImperialArmour(hoa::protocol::clientgm::ImperialArmour* p);
    virtual void handleChaosArmour(hoa::protocol::clientgm::ChaosArmour* p);
    virtual void handleNavigateToggle(hoa::protocol::clientgm::NavigateToggle* p);
    virtual void handleServerOpenToUsersToggle(hoa::protocol::clientgm::ServerOpenToUsersToggle* p);
    virtual void handleTurnOffServer(hoa::protocol::clientgm::TurnOffServer* p);
    virtual void handleTurnCriminal(hoa::protocol::clientgm::TurnCriminal* p);
    virtual void handleResetFactions(hoa::protocol::clientgm::ResetFactions* p);
    virtual void handleRemoveCharFromGuild(hoa::protocol::clientgm::RemoveCharFromGuild* p);
    virtual void handleRequestCharMail(hoa::protocol::clientgm::RequestCharMail* p);
    virtual void handleAlterMail(hoa::protocol::clientgm::AlterMail* p);
    virtual void handleAlterName(hoa::protocol::clientgm::AlterName* p);
    virtual void handleToggleCentinelActivated(hoa::protocol::clientgm::ToggleCentinelActivated* p);
    virtual void handleDoBackUp(hoa::protocol::clientgm::DoBackUp* p);
    virtual void handleShowGuildMessages(hoa::protocol::clientgm::ShowGuildMessages* p);
    virtual void handleSaveMap(hoa::protocol::clientgm::SaveMap* p);
    virtual void handleChangeMapInfoPK(hoa::protocol::clientgm::ChangeMapInfoPK* p);
    virtual void handleChangeMapInfoBackup(hoa::protocol::clientgm::ChangeMapInfoBackup* p);
    virtual void handleChangeMapInfoRestricted(hoa::protocol::clientgm::ChangeMapInfoRestricted* p);
    virtual void handleChangeMapInfoNoMagic(hoa::protocol::clientgm::ChangeMapInfoNoMagic* p);
    virtual void handleChangeMapInfoNoInvi(hoa::protocol::clientgm::ChangeMapInfoNoInvi* p);
    virtual void handleChangeMapInfoNoResu(hoa::protocol::clientgm::ChangeMapInfoNoResu* p);
    virtual void handleChangeMapInfoLand(hoa::protocol::clientgm::ChangeMapInfoLand* p);
    virtual void handleChangeMapInfoZone(hoa::protocol::clientgm::ChangeMapInfoZone* p);
    virtual void handleChangeMapInfoStealNpc(hoa::protocol::clientgm::ChangeMapInfoStealNpc* p);
    virtual void handleChangeMapInfoNoOcultar(hoa::protocol::clientgm::ChangeMapInfoNoOcultar* p);
    virtual void handleChangeMapInfoNoInvocar(hoa::protocol::clientgm::ChangeMapInfoNoInvocar* p);
    virtual void handleSaveChars(hoa::protocol::clientgm::SaveChars* p);
    virtual void handleCleanSOS(hoa::protocol::clientgm::CleanSOS* p);
    virtual void handleShowServerForm(hoa::protocol::clientgm::ShowServerForm* p);
    virtual void handleNight(hoa::protocol::clientgm::Night* p);
    virtual void handleKickAllChars(hoa::protocol::clientgm::KickAllChars* p);
    virtual void handleReloadNPCs(hoa::protocol::clientgm::ReloadNPCs* p);
    virtual void handleReloadServerIni(hoa::protocol::clientgm::ReloadServerIni* p);
    virtual void handleReloadSpells(hoa::protocol::clientgm::ReloadSpells* p);
    virtual void handleReloadObjects(hoa::protocol::clientgm::ReloadObjects* p);
    virtual void handleRestart(hoa::protocol::clientgm::Restart* p);
    virtual void handleResetAutoUpdate(hoa::protocol::clientgm::ResetAutoUpdate* p);
    virtual void handleChatColor(hoa::protocol::clientgm::ChatColor* p);
    virtual void handleIgnored(hoa::protocol::clientgm::Ignored* p);
    virtual void handleCheckSlot(hoa::protocol::clientgm::CheckSlot* p);
    virtual void handleSetIniVar(hoa::protocol::clientgm::SetIniVar* p);
    virtual void handleCreatePretorianClan(hoa::protocol::clientgm::CreatePretorianClan* p);
    virtual void handleRemovePretorianClan(hoa::protocol::clientgm::RemovePretorianClan* p);
    virtual void handleEnableDenounces(hoa::protocol::clientgm::EnableDenounces* p);
    virtual void handleShowDenouncesList(hoa::protocol::clientgm::ShowDenouncesList* p);
    virtual void handleMapMessage(hoa::protocol::clientgm::MapMessage* p);
    virtual void handleSetDialog(hoa::protocol::clientgm::SetDialog* p);
    virtual void handleImpersonate(hoa::protocol::clientgm::Impersonate* p);
    virtual void handleImitate(hoa::protocol::clientgm::Imitate* p);
    virtual void handleRecordAdd(hoa::protocol::clientgm::RecordAdd* p);
    virtual void handleRecordRemove(hoa::protocol::clientgm::RecordRemove* p);
    virtual void handleRecordAddObs(hoa::protocol::clientgm::RecordAddObs* p);
    virtual void handleRecordListRequest(hoa::protocol::clientgm::RecordListRequest* p);
    virtual void handleRecordDetailsRequest(hoa::protocol::clientgm::RecordDetailsRequest* p);
    virtual void handleAlterGuildName(hoa::protocol::clientgm::AlterGuildName* p);
    virtual void handleHigherAdminsMessage(hoa::protocol::clientgm::HigherAdminsMessage* p);

private:
    const int UserIndex;
};

class HoAPacketHandler : public hoa::protocol::PacketHandler {
public:
	HoAPacketHandler(int UI) : UserIndex(UI), clientPacketHandler(UI) {}

    virtual ~HoAPacketHandler() {};
    virtual hoa::protocol::client::ClientPacketHandler* getPacketHandlerClientPacket();
    virtual hoa::protocol::clientgm::ClientGMPacketHandler* getPacketHandlerClientGMPacket();
    virtual hoa::protocol::server::ServerPacketHandler* getPacketHandlerServerPacket();

private:
    const int UserIndex;
    HoAClientPacketHandler clientPacketHandler;
};

void InitProtocolHandler();


#endif /* PROTOCOLHANDLER_H_ */
