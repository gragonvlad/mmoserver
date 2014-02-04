/*
---------------------------------------------------------------------------------------
This source file is part of SWG:ANH (Star Wars Galaxies - A New Hope - Server Emulator)

For more information, visit http://www.swganh.com

Copyright (c) 2006 - 2014 The SWG:ANH Team
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

#ifndef ANH_ZONESERVER_CONTAINER_OBJECT_FACTORY_H
#define ANH_ZONESERVER_CONTAINER_OBJECT_FACTORY_H

#include "ZoneServer/Objects/FactoryBase.h"
#include "ZoneServer/Objects/ObjectFactoryCallback.h"

#define	 gContainerFactory	ContainerObjectFactory::getSingletonPtr()

//=============================================================================

class Container;
namespace swganh	{
namespace database	{
class Database;
class DataBinding;
}}
class DispatchClient;
class TangibleFactory;

//=============================================================================

enum CFQuery
{
    CFQuery_MainData	= 1,
    CFQuery_ObjectCount = 2,
    CFQuery_Objects		= 3
};

//=============================================================================

class ContainerObjectFactory : public FactoryBase, public ObjectFactoryCallback
{
public:

    static ContainerObjectFactory*	getSingletonPtr() {
        return mSingleton;
    }
    static ContainerObjectFactory*	Init(swganh::app::SwganhKernel*	kernel);
    static inline void destroySingleton(void)
    {
        if (mSingleton)
        {
            delete mSingleton;
            mSingleton = 0;
        }
    }

    virtual void	handleObjectReady(Object* object,DispatchClient* client);
    void			handleDatabaseJobComplete(void* ref,swganh::database::DatabaseResult* result);
    void			requestObject(ObjectFactoryCallback* ofCallback,uint64 id,uint16 subGroup,uint16 subType,DispatchClient* client);



private:
    ~ContainerObjectFactory();
    ContainerObjectFactory(swganh::app::SwganhKernel*	kernel);

    void _setupDatabindings();
    void _destroyDatabindings();

    Container* _createContainer(swganh::database::DatabaseResult* result);

    TangibleFactory* mTangibleFactory;

    static ContainerObjectFactory*	mSingleton;


    swganh::database::DataBinding* mContainerBinding;
};

//=============================================================================

class CFAsyncContainer
{
public:

    CFAsyncContainer(ObjectFactoryCallback* of,CFQuery qt,DispatchClient* cl) {
        ofCallback = of;
        client = cl;
        queryType = qt;
    }

    DispatchClient*			client;
    ObjectFactoryCallback*	ofCallback;
    Container*				container;
    CFQuery					queryType;
};

//=============================================================================


#endif

