#include "EventMan.h"
#include "..\Public\Event.h"


void EventMan::Init()
{
	EventRunning = false;
	CurrentEvent = nullptr;
}

EventMan& EventMan::Get()
{
	static EventMan instance;
	return instance;
}

void EventMan::AddEvent(Event* event)
{
	Log::GetLog()->warn("EventMan::AddEvent(Event event)");
	Events.push_back(event);
}

void EventMan::RemoveEvent(Event* event)
{
	if (CurrentEvent != nullptr && CurrentEvent == event)
	{

		EventRunning = false;
		CurrentEvent = nullptr;
		NextEventTime = timeGetTime() + 300000;
	}
	EventItr Itr = std::find(Events.begin(), Events.end(), event);
	if (Itr != Events.end()) Events.erase(Itr);
}


bool EventMan::AddPlayer(long long PlayerID, AShooterPlayerController* player)
{
	if (IsEventRunning() && CurrentEvent != nullptr && CurrentEvent->GetState() == EventState::WaitingForPlayers && FindPlayer(PlayerID) == nullptr)
	{
		Players.push_back(EventPlayer(PlayerID, player));
		return true;
	}
	return false;
}

EventPlayer* EventMan::FindPlayer(long long PlayerID)
{
	EventPlayerArrayItr Itr = std::find_if(Players.begin(), Players.end(), [PlayerID](EventPlayer p) -> bool { return p.PlayerID == PlayerID; });
	if (Itr != Players.end()) return &(*Itr);
	return nullptr;
}

bool EventMan::RemovePlayer(long long PlayerID, bool ByCommand)
{
	if (IsEventRunning() && CurrentEvent != nullptr && (CurrentEvent->GetState() == EventState::WaitingForPlayers && ByCommand || !ByCommand))
	{
		EventPlayerArrayItr Itr = std::find_if(Players.begin(), Players.end(), [PlayerID](EventPlayer p) -> bool { return p.PlayerID == PlayerID; });
		if (Itr != Players.end())
		{
			Players.erase(Itr);
			return true;
		}
	}
	return false;
}

bool EventMan::StartEvent(const int EventID)
{
	if (CurrentEvent != nullptr) return false;
	if (EventID == -1)
	{
		int Rand = 0;
		CurrentEvent = Events[Rand];
		CurrentEvent->InitConfig(JoinEventCommand, ServerName, Map);
		EventRunning = true;
	}
	else if (EventID < Events.size())
	{
		CurrentEvent = Events[EventID];
		CurrentEvent->InitConfig(JoinEventCommand, ServerName, Map);
		EventRunning = true;
	}
	return true;
}

void EventMan::Update()
{
	if (Events.size() == 0) return;
	if (IsEventRunning() && CurrentEvent != nullptr)
	{
		if (CurrentEvent->GetState() != Finished) CurrentEvent->Update();
		else
		{
			EventRunning = false;
			CurrentEvent = nullptr;
			NextEventTime = timeGetTime() + 300000;
		}
	}
	else if (timeGetTime() > NextEventTime)
	{
		if (Map.IsEmpty()) ArkApi::GetApiUtils().GetShooterGameMode()->GetMapName(&Map);
		StartEvent();
		NextEventTime = timeGetTime() + 0;//RandomNumber(7200000, 21600000);
	}
}

void EventMan::TeleportEventPlayers(const bool TeamBased, const bool WipeInventory, const bool PreventDinos, SpawnsMap Spawns, const int StartTeam)
{
	int TeamCount = (int)Spawns.size(), TeamIndex = StartTeam;
	TArray<int> SpawnIndexs;
	for (int i = 0; i < TeamCount; i++) SpawnIndexs.Add(0);
	FVector Pos;
	for (EventPlayerArrayItr itr = Players.begin(); itr != Players.end(); itr++)
	{
		SpawnsMapItr SpawnItr = Spawns.find(TeamIndex);
		if (SpawnItr != Spawns.end())
		{
			if (itr->ASPC && itr->ASPC->PlayerStateField()() && itr->ASPC->GetPlayerCharacter() && !itr->ASPC->GetPlayerCharacter()->IsDead() && (!PreventDinos && itr->ASPC->GetPlayerCharacter()->GetRidingDino() != nullptr || itr->ASPC->GetPlayerCharacter()->GetRidingDino() == nullptr))
			{
				if (WipeInventory)
				{
					UShooterCheatManager* cheatManager = static_cast<UShooterCheatManager*>(itr->ASPC->CheatManagerField()());
					if (cheatManager) cheatManager->ClearPlayerInventory((int)itr->ASPC->LinkedPlayerIDField()(), true, true, true);
				}

				itr->StartPos = ArkApi::GetApiUtils().GetPosition(itr->ASPC);

				Pos = SpawnItr->second[SpawnIndexs[TeamIndex]++];
				itr->Team = TeamIndex;
				itr->ASPC->SetPlayerPos(Pos.X, Pos.Y, Pos.Z);

				if (TeamCount > 1)
				{
					TeamIndex++;
					if (TeamIndex == TeamCount) TeamIndex = StartTeam;
				}

				if (SpawnIndexs[TeamIndex] == SpawnItr->second.Num()) SpawnIndexs[TeamIndex] = 0;
			}
			else itr = Players.erase(itr);
		}
	}
}

void EventMan::TeleportWinningEventPlayersToStart()
{
	for (EventPlayerArrayItr itr = Players.begin(); itr != Players.end(); itr++)
	{
		itr->ASPC->SetPlayerPos(itr->StartPos.X, itr->StartPos.Y, itr->StartPos.Z);
		itr = Players.erase(itr);
	}
}

bool EventMan::CanTakeDamage(long long AttackerID, long long VictimID)
{	
	if (CurrentEvent == nullptr || AttackerID == VictimID) return true;
	EventPlayer* Attacker, *Victim;
	if ((Attacker = FindPlayer(AttackerID)) != nullptr && (Victim = FindPlayer(VictimID)) != nullptr)
	{
		if (Attacker->Team != 0 && Attacker->Team == Victim->Team) return false;
		else if (CurrentEvent->GetState() != EventState::Fighting) return false;
	}
	return true;
}

void EventMan::OnPlayerDied(long long AttackerID, long long VictimID)
{
	if (AttackerID != -1 && AttackerID != VictimID)
	{
		EventPlayer* Attacker;
		if ((Attacker = FindPlayer(AttackerID)) != nullptr) Attacker->Kills++;
	}
	RemovePlayer(VictimID, false);
}

void EventMan::OnPlayerLogg(AShooterPlayerController* Player)
{
	if (CurrentEvent != nullptr && CurrentEvent->GetState() > EventState::TeleportingPlayers)
	{
		EventPlayer* EPlayer;
		if ((EPlayer = FindPlayer(Player->LinkedPlayerIDField()())) != nullptr)
		{
			if (CurrentEvent->KillOnLoggout()) Player->ServerSuicide_Implementation();
			else Player->SetPlayerPos(EPlayer->StartPos.X, EPlayer->StartPos.Y, EPlayer->StartPos.Z);
			RemovePlayer(Player->LinkedPlayerIDField()(), false);
		}
	}
}

bool EventMan::IsEventProtectedStructure(const FVector& StructurePos)
{
	for (Event* Evt : Events) if (Evt->IsEventProtectedStructure(StructurePos)) return true;
	return false;
}