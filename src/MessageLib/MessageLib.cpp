/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2010 The SWG:ANH Team
---------------------------------------------------------------------------------------
Use of this source code is governed by the GPL v3 license that can be found
in the COPYING file or at http://www.gnu.org/licenses/gpl-3.0.html

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
---------------------------------------------------------------------------------------
*/

#include "MessageLib.h"

#include "ZoneServer/BuildingObject.h"
#include "ZoneServer/CellObject.h"
#include "ZoneServer/CharSheetManager.h"
#include "ZoneServer/Conversation.h"
#include "ZoneServer/CraftingTool.h"
#include "ZoneServer/CurrentResource.h"
#include "ZoneServer/Datapad.h"
#include "ZoneServer/HouseObject.h"
#include "ZoneServer/InTangibleObject.h"
#include "ZoneServer/HarvesterObject.h"
#include "ZoneServer/FactoryObject.h"
#include "ZoneServer/FactoryCrate.h"
#include "ZoneServer/Inventory.h"
#include "ZoneServer/ManufacturingSchematic.h"

#include "ZoneServer/NPCObject.h"
#include "ZoneServer/ObjectControllerOpcodes.h"
#include "ZoneServer/ObjectFactory.h"
#include "ZoneServer/PlayerObject.h"
#include "ZoneServer/ResourceContainer.h"
#include "ZoneServer/UIOpcodes.h"
#include "ZoneServer/VehicleController.h"
#include "ZoneServer/Wearable.h"
#include "ZoneServer/WorldConfig.h"
#include "ZoneServer/WorldManager.h"
#include "ZoneServer/ZoneOpcodes.h"
#include "ZoneServer/Zmap.h"

#include "LogManager/LogManager.h"

#include "Common/atMacroString.h"
#include "Common/DispatchClient.h"
#include "Common/Message.h"
#include "Common/MessageDispatch.h"
#include "Common/MessageFactory.h"
#include "Common/MessageOpcodes.h"

#include <boost/lexical_cast.hpp>

//======================================================================================================================

bool		MessageLib::mInsFlag    = false;
MessageLib*	MessageLib::mSingleton  = NULL;

//======================================================================================================================

MessageLib::MessageLib()
{
	mMessageFactory = gMessageFactory;
}

//======================================================================================================================

MessageLib*	MessageLib::Init()
{
	if(!mInsFlag)
	{
		mSingleton = new MessageLib();
		mInsFlag = true;

		return mSingleton;
	}
	else
		return mSingleton;
}

//======================================================================================================================

MessageLib::~MessageLib()
{
	mInsFlag = false;
	delete(mSingleton);
}

//======================================================================================================================
//
// Checks the validity of the player in the global map
//
bool MessageLib::_checkPlayer(const PlayerObject* const player) const
{	
	//player gets PlayerConnState_LinkDead when he disconnects but is still in the world
	//we in theory could still send updates 
	//return((player->isConnected())&&(player->getClient()));
 
	//the idea is that this check gets useless when the SI / knownobjectscode is stable

	return((player)&&(player->getClient()));
}

//======================================================================================================================

bool MessageLib::_checkPlayer(uint64 playerId) const
{
	PlayerObject* tested = dynamic_cast <PlayerObject*> (gWorldManager->getObjectById(playerId));

	if(!tested)
	{
		gLogger->log(LogManager::NOTICE,"Player Id (%I64u) invalid",playerId);
		return false;
	}

	return((tested->isConnected())&&(tested->getClient()));
}

//================================================================================================0
//send movement based on messageheap size and distance
bool MessageLib::_checkDistance(const glm::vec3& mPosition1, Object* object, uint32 heapWarningLevel)
{
	
	//just send everything we have
	if(heapWarningLevel < 4)
		return true;
	else
	if (heapWarningLevel < 6)
	{
        if(glm::distance(object->mPosition, mPosition1) < 96)
			return object->movementMessageToggle();
	}
	else
	if (heapWarningLevel < 8)
	{
        if(glm::distance(object->mPosition, mPosition1) < 64)
			return object->movementMessageToggle();
	}
	else
	if (heapWarningLevel < 10)
	{
		float distance = glm::distance(object->mPosition, mPosition1);
		if(distance <= 32)
			return true;
		else
		if(distance > 32)
			return object->movementMessageToggle();
		else
		if(distance > 64)
			return false;
	}
	else
	if (heapWarningLevel >= 10)
		return false;
	
	
	

	return false;
}

//======================================================================================================================
//
// broadcasts a message to all players in range of the given object
// this is slow - better use registered Players

void MessageLib::_sendToInRangeUnreliable(Message* message, Object* const object,uint16 priority,bool toSelf)
{
	
	glm::vec3   position;
	
	//cater for players in cells
	if (object->getParentId())
	{
		position = object->getWorldPosition(); 
	}
	else
	{
		position = object->mPosition;
	}
	
	ObjectListType inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	bool failed = false;

	
	//save us some cycles if traffic is low

	if(mMessageFactory->HeapWarningLevel() <= 4)
	{
		for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
		{
 			PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
			if(player && _checkPlayer(player))
			{
				gLogger->log(LogManager::DEBUG,"MessageLib::_sendToInRangeUnreliable send unreliable Message to payer %I64u",player->getId());
 				// clone our message
 				mMessageFactory->StartMessage();
 				mMessageFactory->addData(message->getData(),message->getSize());
 
 				(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));		
 			}
			else
			{
				//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
				assert(false && "Invalid Player in sendtoInrange");
				failed = true;
 			}				
 		}
 
		if( failed)
			gLogger->log(LogManager::NOTICE,"MessageLib Heap Protection engaged Heap Warning Level %u Heap size %f",mMessageFactory->HeapWarningLevel(),mMessageFactory->getHeapsize());
	}
	else
	{
		for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
		{
			PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
			if(_checkPlayer(player))
			{
				bool yn = _checkDistance(player->mPosition,object,mMessageFactory->HeapWarningLevel());
				if(yn)
				{
					// clone our message
					mMessageFactory->StartMessage();
					mMessageFactory->addData(message->getData(),message->getSize());
	
					(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
				}
				else
				{
					failed = true;
				}
			}	
		}
	}

	if(toSelf)
	{
		const PlayerObject* const srcPlayer = dynamic_cast<const PlayerObject*>(object);

		if(_checkPlayer(srcPlayer))
		{
			(srcPlayer->getClient())->SendChannelAUnreliable(message,srcPlayer->getAccountId(),CR_Client,static_cast<uint8>(priority));
			return;
		}
	}

	mMessageFactory->DestroyMessage(message);
}

//======================================================================================================================
//
// broadcasts a message to all players in range of the given object
//
void MessageLib::_sendToInRangeUnreliable(Message* message, PlayerObject* const player,uint16 priority,bool toSelf)
{
	
	glm::vec3   position;
	
	//cater for players in cells
	if (player->getParentId())
	{
		position = player->getWorldPosition(); 
	}
	else
	{
		position = player->mPosition;
	}
	
	gSpatialIndexManager->sendToRegisteredPlayers(player, [this, priority, message, player, toSelf] (PlayerObject* recipient)
		{
		
			bool failed = false;

			//save us some cycles if traffic is low

			if(mMessageFactory->HeapWarningLevel() <= 4)
			{
				
				if(_checkPlayer(recipient))
					{

 						// clone our message
 						mMessageFactory->StartMessage();
 						mMessageFactory->addData(message->getData(),message->getSize());
 
 						(recipient->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));		
 					}
				else
					{
						//an invalid player at this point is like armageddon and Ultymas birthday combined at one time
						//if this happens we need to know about it
						assert(false && "Invalid Player in sendtoInrange");
						failed = true;
 					}
	 
				if( failed)
					gLogger->log(LogManager::NOTICE,"MessageLib Heap Protection engaged Heap Warning Level %u Heap size %f",mMessageFactory->HeapWarningLevel(),mMessageFactory->getHeapsize());
			}
			else
			{	
				if(_checkPlayer(recipient))
				{
					bool yn = _checkDistance(recipient->mPosition,player,mMessageFactory->HeapWarningLevel());
					if(yn)
					{
						// clone our message
						mMessageFactory->StartMessage();
						mMessageFactory->addData(message->getData(),message->getSize());
	
						(recipient->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),recipient->getAccountId(),CR_Client,static_cast<uint8>(priority));
					}
					else
					{
						failed = true;
					}
				
				}
	
			}
	

			if(toSelf)
			{

				if(_checkPlayer(player))
				{
					(player->getClient())->SendChannelAUnreliable(message,player->getAccountId(),CR_Client,static_cast<uint8>(priority));
					return;
				}
			}

			mMessageFactory->DestroyMessage(message);
		}
	);
}

void MessageLib::_sendToInRangeUnreliableChat(Message* message, const CreatureObject* object,uint16 priority, uint32 crc)
{
	
	glm::vec3   position;
	
	//cater for players in cells
	if (object->getParentId())
	{
		position = object->getWorldPosition(); 
	}
	else
	{
		position = object->mPosition;
	}
	
	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	Message* clonedMessage;
	bool failed = false;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if(_checkPlayer(player) && (!player->checkIgnoreList(crc)))
		{
 			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 			clonedMessage = mMessageFactory->EndMessage();
		
			// replace the target id
			int8* data = clonedMessage->getData() + 12;
			*((uint64*)data) = player->getId();
			(player->getClient())->SendChannelAUnreliable(clonedMessage,player->getAccountId(),CR_Client,5);
		} 		
	}
}

void MessageLib::_sendToInRangeUnreliableChatGroup(Message* message, const CreatureObject* object,uint16 priority, uint32 crc)
{
	
	glm::vec3   position;
	
	//cater for players in cells
	if (object->getParentId())
	{
		position = object->getWorldPosition(); 
	}
	else
	{
		position = object->mPosition;
	}
	
	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	Message* clonedMessage;
	bool failed = false;

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{		
		PlayerObject* player = dynamic_cast<PlayerObject*>((*playerIt));
		if ((_checkPlayer(player)) && (object->getGroupId()) &&(player->getGroupId() == object->getGroupId())&&(!player->checkIgnoreList(crc)))
		{
 			// clone our message
 			mMessageFactory->StartMessage();
 			mMessageFactory->addData(message->getData(),message->getSize());
 			clonedMessage = mMessageFactory->EndMessage();
		
			// replace the target id
			int8* data = clonedMessage->getData() + 12;
			*((uint64*)data) = player->getId();
			(player->getClient())->SendChannelAUnreliable(clonedMessage,player->getAccountId(),CR_Client,5);
		} 		
	}

}

//======================================================================================================================

void MessageLib::_sendToInRange(Message* message, Object* const object,uint16 priority,bool toSelf)
{
	glm::vec3   position;
	
	//cater for players in cells
	if (object->getParentId())
	{
		position = object->getWorldPosition(); 
	}
	else
	{
		position = object->mPosition;
	}

	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		if(_checkPlayer(player))
		{
			// clone our message
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
		}


	}

	if(toSelf)
	{
		const PlayerObject* const srcPlayer = dynamic_cast<const PlayerObject*>(object);

		if(_checkPlayer(srcPlayer))
		{
			(srcPlayer->getClient())->SendChannelA(message,srcPlayer->getAccountId(),CR_Client, static_cast<uint8>(priority));
			return;
		}
	}

	mMessageFactory->DestroyMessage(message);
}

//======================================================================================================================
//
// Broadcasts a message to players in group and in range of the given object, used by tutorial and other instances
//
void MessageLib::_sendToInstancedPlayers(Message* message,uint16 priority, const PlayerObject* const playerObject) const
{
	if (!_checkPlayer(playerObject))
	{
		mMessageFactory->DestroyMessage(message);
		return;
	}

	glm::vec3   position;
	
	//cater for players in cells
	if (playerObject->getParentId())
	{
		position = playerObject->getWorldPosition(); 
	}
	else
	{
		position = playerObject->mPosition;
	}

	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
	{
		PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		if (_checkPlayer(player))
		{
			// Clone the message.
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
		}

	}

	mMessageFactory->DestroyMessage(message);
}

//======================================================================================================================
//
// Broadcasts a message to players in group and in range of the given object, used by tutorial and other instances
//
void MessageLib::_sendToInstancedPlayersUnreliable(Message* message,uint16 priority, const PlayerObject* const playerObject) const
{
	if (!_checkPlayer(playerObject))
	{
		mMessageFactory->DestroyMessage(message);
		return;
	}

	glm::vec3   position;
	
	//cater for players in cells
	if (playerObject->getParentId())
	{
		position = playerObject->getWorldPosition(); 
	}
	else
	{
		position = playerObject->mPosition;
	}

	ObjectListType		inRangePlayers;
	mGrid->GetPlayerViewingRangeCellContents(mGrid->getCellId(position.x, position.z), &inRangePlayers);

	if(playerObject->getGroupId() != 0)
	{
		for(std::list<Object*>::iterator playerIt = inRangePlayers.begin(); playerIt != inRangePlayers.end(); playerIt++)
		{
			PlayerObject* player = dynamic_cast<PlayerObject*>(*playerIt);
		
			if((playerObject->getGroupId() != 0) && (player->getGroupId() != playerObject->getGroupId()))
				continue;
		
			if (_checkPlayer(player))
			{
				// Clone the message.
				mMessageFactory->StartMessage();
				mMessageFactory->addData(message->getData(),message->getSize());

				(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}

		}
	}

	mMessageFactory->DestroyMessage(message);
}

//======================================================================================================================
//
// broadcasts a message to all players on the current zone
//
void MessageLib::_sendToAll(Message* message,uint16 priority,bool unreliable) const
{
	const PlayerAccMap* const		players		= gWorldManager->getPlayerAccMap();
	PlayerAccMap::const_iterator	playerIt	= players->begin();

	while(playerIt != players->end())
	{
		const PlayerObject* const player = (*playerIt).second;

		if(_checkPlayer(player))
		{
			mMessageFactory->StartMessage();
			mMessageFactory->addData(message->getData(),message->getSize());

			if(unreliable)
			{
				(player->getClient())->SendChannelAUnreliable(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}
			else
			{
				(player->getClient())->SendChannelA(mMessageFactory->EndMessage(),player->getAccountId(),CR_Client,static_cast<uint8>(priority));
			}
		}

		++playerIt;
	}

	mMessageFactory->DestroyMessage(message);
}


//======================================================================================================================
//
// creates all items childobjects
//
/*
bool MessageLib::sendItemChildren(TangibleObject* srcObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	ObjectIDList*			childObjects		= srcObject->getObjects();
	ObjectIDList::iterator	childObjectsIt		= childObjects->begin();

	while(childObjectsIt != childObjects->end())
	{
		// items
		if(TangibleObject* to = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById((*childObjectsIt))))
		{
			gMessageLib->sendCreateTangible(to,targetObject);
		}

		++childObjectsIt;
	}

	return(true);
}
*/
//======================================================================================================================
//
// create player
//
bool MessageLib::sendCreatePlayer(PlayerObject* playerObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(playerObject,targetObject,false);

	if(targetObject == playerObject)
	{
		sendBaselinesCREO_1(playerObject);
		sendBaselinesCREO_4(playerObject);
	}

	sendBaselinesCREO_3(playerObject,targetObject);
	sendBaselinesCREO_6(playerObject,targetObject);

	sendCreateObjectByCRC(playerObject,targetObject,true);
	sendContainmentMessage(playerObject->getPlayerObjId(),playerObject->getId(),4,targetObject);

	sendBaselinesPLAY_3(playerObject,targetObject);
	sendBaselinesPLAY_6(playerObject,targetObject);

	if(targetObject == playerObject)
	{
		sendBaselinesPLAY_8(playerObject,targetObject);
		sendBaselinesPLAY_9(playerObject,targetObject);
	}

	//close the yalp
	sendEndBaselines(playerObject->getPlayerObjId(),targetObject);

	sendPostureMessage(playerObject,targetObject);


    if(playerObject->getParentId())
    {
        sendContainmentMessage(playerObject->getId(),playerObject->getParentId(),4,targetObject);
    }

	//===================================================================================
	// create inventory, datapad, hair, MissionBag and equipped items get created for the player only !!
	// equipped items for other watchers are handled via the equiplists

    

        
    //equipped items are already in the creo6 so only send them for ourselves

    sendEndBaselines(playerObject->getId(),targetObject);

    return(true);
}

//======================================================================================================================
//
// create creature
//
bool MessageLib::sendCreateCreature(CreatureObject* creatureObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(creatureObject,targetObject,false);

	sendBaselinesCREO_3(creatureObject,targetObject);
	sendBaselinesCREO_6(creatureObject,targetObject);

	if(creatureObject->getParentId() && creatureObject->getCreoGroup() != CreoGroup_Vehicle)
	{
		sendContainmentMessage(creatureObject->getId(),creatureObject->getParentId(),0xffffffff,targetObject);
	}

	sendEndBaselines(creatureObject->getId(),targetObject);

	sendUpdatePvpStatus(creatureObject,targetObject);

	sendPostureMessage(creatureObject,targetObject);

	return(true);
}


//======================================================================================================================

bool MessageLib::sendCreateStaticObject(TangibleObject* tangibleObject,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject) || !tangibleObject)
	{
		gLogger->log(LogManager::DEBUG,"MessageLib::sendCreateStaticObject No valid player");
		return(false);
	}
	
	sendCreateObjectByCRC(tangibleObject,targetObject,false);
	sendBaselinesSTAO_3(tangibleObject,targetObject);
	sendBaselinesSTAO_6(tangibleObject,targetObject);
	sendEndBaselines(tangibleObject->getId(),targetObject);

	return true;
}

//======================================================================================================================
//
// create intangible 
//
bool MessageLib::sendCreateInTangible(IntangibleObject* intangibleObject,uint64 containmentId,PlayerObject* targetObject) 
{
	if(!_checkPlayer(targetObject) || !intangibleObject)
	{
		gLogger->log(LogManager::DEBUG,"MessageLib::sendCreateInTangible No valid player");
		return(false);
	}

	gMessageLib->sendCreateObjectByCRC(intangibleObject,targetObject,false);
	gMessageLib->sendBaselinesITNO_3(intangibleObject,targetObject);
	gMessageLib->sendBaselinesITNO_6(intangibleObject,targetObject);
	gMessageLib->sendBaselinesITNO_8(intangibleObject,targetObject);
	gMessageLib->sendBaselinesITNO_9(intangibleObject,targetObject);
	gMessageLib->sendContainmentMessage(intangibleObject->getId(), containmentId, 0xffffffff, targetObject);
	gMessageLib->sendEndBaselines(intangibleObject->getId(),targetObject);

	return true;
}

//======================================================================================================================
//
// create tangible Object in the world
//
bool MessageLib::sendCreateTano(TangibleObject* tangibleObject,PlayerObject* targetObject) 
{
	if(!_checkPlayer(targetObject))
	{
		gLogger->log(LogManager::DEBUG,"MessageLib::sendCreateTangible No valid player");
		return(false);
	}

	
	uint64 parentId = tangibleObject->getParentId();

	sendCreateObjectByCRC(tangibleObject,targetObject,false);

	if(parentId != 0)
	{
		// its in a cell, container, inventory
		if(parentId != targetObject->getId())
		{
			// could be inside a crafting tool
			Object* parent = gWorldManager->getObjectById(parentId);
			CreatureObject* creatureObject = dynamic_cast<CreatureObject*>(parent);

			if(parent && dynamic_cast<CraftingTool*>(parent))
			{
				sendContainmentMessage(tangibleObject->getId(),parentId,0,targetObject);
			}
			// if equipped, also tie it to the object
			else if(creatureObject)
			{
				Item* item = dynamic_cast<Item*>(tangibleObject);
				sendContainmentMessage(tangibleObject->getId(),creatureObject->getId(),4,targetObject);				
			}
			else
			{
				sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),0xffffffff,targetObject);
			}
		}
		// or tied directly to an object
		else
		{
			sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),4,targetObject);
		}
	}
	else
	{
		sendContainmentMessage(tangibleObject->getId(),tangibleObject->getParentId(),0xffffffff,targetObject);
	}

	sendBaselinesTANO_3(tangibleObject,targetObject);
	sendBaselinesTANO_6(tangibleObject,targetObject);

	sendEndBaselines(tangibleObject->getId(),targetObject);

	return(true);
}


//======================================================================================================================
//
// create resource container
//
bool MessageLib::sendCreateResourceContainer(ResourceContainer* resourceContainer,PlayerObject* targetObject)
{
	if(!_checkPlayer(targetObject))
		return(false);

	sendCreateObjectByCRC(resourceContainer,targetObject,false);

	uint64 parentId = resourceContainer->getParentId();

	sendContainmentMessage(resourceContainer->getId(),parentId,0xffffffff,targetObject);	
	
	sendBaselinesRCNO_3(resourceContainer,targetObject);
	sendBaselinesRCNO_6(resourceContainer,targetObject);

	sendBaselinesRCNO_8(resourceContainer,targetObject);
	sendBaselinesRCNO_9(resourceContainer,targetObject);

	sendEndBaselines(resourceContainer->getId(),targetObject);

	return(true);
}

//======================================================================================================================
//
// create building
//
bool MessageLib::sendCreateBuilding(BuildingObject* buildingObject,PlayerObject* playerObject)
{
	if(!_checkPlayer(playerObject))
		return(false);

	bool publicBuilding = true;

	//test buildings on house basis here
	//perhaps move to on cell basis sometime ?
	if(HouseObject* house = dynamic_cast<HouseObject*>(buildingObject))
	{
		house->checkCellPermission(playerObject);
		publicBuilding = buildingObject->getPublic();
	}

	sendCreateObjectByCRC(buildingObject,playerObject,false);

	sendBaselinesBUIO_3(buildingObject,playerObject);
	sendBaselinesBUIO_6(buildingObject,playerObject);

	uint64 buildingId = buildingObject->getId();

	CellObjectList*				cellList	= buildingObject->getCellList();
	CellObjectList::iterator	cellIt		= cellList->begin();

	uint64 cellCount = cellList->size();
	while(cellIt != cellList->end())
	{
		CellObject* cell = (*cellIt);
		uint64 cellId = cell->getId();

		uint64 count = buildingObject->getMinCellId()-1;
		sendCreateObjectByCRC(cell,playerObject,false);
		sendContainmentMessage(cellId,buildingId,0xffffffff,playerObject);

		//cell ids are id based for tutorial cells!
		if(cell->getId() <= 2203318222975)
		{
			sendBaselinesSCLT_3(cell,cellId - buildingId,playerObject);
		}
		else
		{
			sendBaselinesSCLT_3(cell,cellId - count,playerObject);
		}
		sendBaselinesSCLT_6(cell,playerObject);

		sendUpdateCellPermissionMessage(cell,publicBuilding,playerObject);	 //cellpermissions get checked by datatransform
		sendEndBaselines(cellId,playerObject);

		++cellIt;
	}

	sendEndBaselines(buildingId,playerObject);

	return(true);
}

//======================================================================================================================
//
// create a harvester
//
bool MessageLib::sendCreateHarvester(HarvesterObject* harvester,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(harvester,player,false);

	sendBaselinesHINO_3(harvester,player);
	sendBaselinesHINO_6(harvester,player);
	sendBaselinesHINO_7(harvester,player);


	sendEndBaselines(harvester->getId(),player);

	//int8 effectStr[400];
	//sprintf(effectStr,"clienteffect/lair_med_damage_smoke.cef");
	//sendPlayClientEffectObjectMessage(effectStr,"",harvester,player);

	return(true);
}

//======================================================================================================================
//
// create a factory
//
bool MessageLib::sendCreateFactory(FactoryObject* factory,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(factory,player,false);

	sendBaselinesINSO_3(factory,player);
	sendBaselinesINSO_6(factory,player);

	TangibleObject* InHopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getIngredientHopper()));
	sendCreateTano(InHopper,player);

	TangibleObject* OutHopper = dynamic_cast<TangibleObject*>(gWorldManager->getObjectById(factory->getOutputHopper()));
	sendCreateTano(OutHopper,player);


	sendEndBaselines(factory->getId(),player);

	//int8 effectStr[400];
	//sprintf(effectStr,"clienteffect/lair_med_damage_smoke.cef");
	//sendPlayClientEffectObjectMessage(effectStr,"",harvester,player);

	return(true);
}

//======================================================================================================================
//
// create a structure
//
bool MessageLib::sendCreateStructure(PlayerStructure* structure,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	if(HarvesterObject* harvester = dynamic_cast<HarvesterObject*>(structure))
	{
		return(sendCreateHarvester(harvester, player));
	}
	else if(HouseObject* house = dynamic_cast<HouseObject*>(structure))
	{
		return(sendCreateBuilding(house, player));
	}
	else if(FactoryObject* factory = dynamic_cast<FactoryObject*>(structure))
	{
		return(sendCreateFactory(factory, player));
	}

	if(structure->getPlayerStructureFamily() == PlayerStructure_Fence)
	{
		return(sendCreateInstallation(structure, player));
	}

	gLogger->log(LogManager::DEBUG,"MessageLib::sendCreateStructure:ID %I64u : couldnt cast structure",structure->getId());

	return(false);
}

//======================================================================================================================
//
// create camp
//
bool MessageLib::sendCreateCamp(TangibleObject* camp,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(camp,player,false);

	sendBaselinesBUIO_3(camp,player);
	sendBaselinesBUIO_6(camp,player);

	uint64 campId = camp->getId();

	sendEndBaselines(campId,player);

	return(true);
}

//======================================================================================================================
//
// create Installation
//
bool MessageLib::sendCreateInstallation(PlayerStructure* structure,PlayerObject* player)
{
	if(!_checkPlayer(player))
		return(false);

	sendCreateObjectByCRC(structure,player,false);

	sendBaselinesINSO_3(structure,player);
	sendBaselinesINSO_6(structure,player);

	uint64 structureId = structure->getId();

	sendEndBaselines(structureId,player);

	return(true);
}

//======================================================================================================================
//
// create manufacturing schematic
// the attributes bool makes the MSCO 3 send the attribute list
//
bool MessageLib::sendCreateManufacturingSchematic(ManufacturingSchematic* manSchem,PlayerObject* playerObject,bool attributes)
{
	if(!_checkPlayer(playerObject))
		return(false);

	sendCreateObjectByCRC(manSchem,playerObject,false);

	// parent should always be a crafting tool for now
	sendContainmentMessage(manSchem->getId(),manSchem->getParentId(),4,playerObject);

	sendBaselinesMSCO_3(manSchem,playerObject,attributes);
	sendBaselinesMSCO_6(manSchem,playerObject);

	sendBaselinesMSCO_8(manSchem,playerObject);
	sendBaselinesMSCO_9(manSchem,playerObject);

	sendEndBaselines(manSchem->getId(),playerObject);

	return(true);
}



//======================================================================================================================



