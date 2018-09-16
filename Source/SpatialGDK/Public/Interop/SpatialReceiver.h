// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#pragma once

#include "CoreMinimal.h"

#include "EngineClasses/SpatialActorChannel.h"
#include "EngineClasses/SpatialNetDriver.h"
#include "EngineClasses/SpatialPackageMapClient.h"
#include "Interop/SpatialTypebindingManager.h"
#include "Schema/StandardLibrary.h"

#include <improbable/c_schema.h>
#include <improbable/c_worker.h>

#include "SpatialReceiver.generated.h"

class USpatialSender;
class UGlobalStateManager;

using FChannelObjectPair = TPair<TWeakObjectPtr<USpatialActorChannel>, TWeakObjectPtr<UObject>>;
using FUnresolvedObjectsMap = TMap<Schema_FieldId, TSet<const UObject*>>;
struct FObjectReferences;
using FObjectReferencesMap = TMap<int32, FObjectReferences>;

struct PendingAddComponentWrapper
{
	PendingAddComponentWrapper() = default;
	PendingAddComponentWrapper(Worker_EntityId InEntityId, Worker_ComponentId InComponentId, const TSharedPtr<SpatialComponent>& InData)
		: EntityId(InEntityId), ComponentId(InComponentId), Data(InData) {}

	Worker_EntityId EntityId;
	Worker_ComponentId ComponentId;
	TSharedPtr<SpatialComponent> Data;
};

struct FObjectReferences
{
	FObjectReferences() = default;
	FObjectReferences(FObjectReferences&& Other)
		: UnresolvedRefs(MoveTemp(Other.UnresolvedRefs))
		, bSingleProp(Other.bSingleProp)
		, Buffer(MoveTemp(Other.Buffer))
		, NumBufferBits(Other.NumBufferBits)
		, Array(MoveTemp(Other.Array))
		, ParentIndex(Other.ParentIndex)
		, Property(Other.Property) {}

	// Single property constructor
	FObjectReferences(const UnrealObjectRef& InUnresolvedRef, int32 InParentIndex, UProperty* InProperty)
		: bSingleProp(true), ParentIndex(InParentIndex), Property(InProperty)
	{
		UnresolvedRefs.Add(InUnresolvedRef);
	}

	// Struct (memory stream) constructor
	FObjectReferences(const TArray<uint8>& InBuffer, int32 InNumBufferBits, const TSet<UnrealObjectRef>& InUnresolvedRefs, int32 InParentIndex, UProperty* InProperty)
		: UnresolvedRefs(InUnresolvedRefs), bSingleProp(false), Buffer(InBuffer), NumBufferBits(InNumBufferBits), ParentIndex(InParentIndex), Property(InProperty) {}

	// Array constructor
	FObjectReferences(FObjectReferencesMap* InArray, int32 InParentIndex, UProperty* InProperty)
		: bSingleProp(false), Array(InArray), ParentIndex(InParentIndex), Property(InProperty) {}

	TSet<UnrealObjectRef>				UnresolvedRefs;

	bool								bSingleProp;
	TArray<uint8>						Buffer;
	int32								NumBufferBits;

	TUniquePtr<FObjectReferencesMap>	Array;
	int32								ParentIndex;
	UProperty*							Property;
};

struct FPendingIncomingRPC
{
	FPendingIncomingRPC(const TSet<UnrealObjectRef>& InUnresolvedRefs, UObject* InTargetObject, UFunction* InFunction, const TArray<uint8>& InPayloadData, int64 InCountBits)
		: UnresolvedRefs(InUnresolvedRefs), TargetObject(InTargetObject), Function(InFunction), PayloadData(InPayloadData), CountBits(InCountBits) {}

	TSet<UnrealObjectRef> UnresolvedRefs;
	TWeakObjectPtr<UObject> TargetObject;
	UFunction* Function;
	TArray<uint8> PayloadData;
	int64 CountBits;
};

using FIncomingRPCArray = TArray<TSharedPtr<FPendingIncomingRPC>>;

UCLASS()
class USpatialReceiver : public UObject
{
	GENERATED_BODY()

public:
	void Init(USpatialNetDriver* NetDriver);

	// Dispatcher Calls
	void OnCriticalSection(bool InCriticalSection);
	void OnAddEntity(Worker_AddEntityOp& Op);
	void OnAddComponent(Worker_AddComponentOp& Op);
	void OnRemoveComponent(Worker_RemoveComponentOp& Op);
	void OnRemoveEntity(Worker_RemoveEntityOp& Op);
	void OnAuthorityChange(Worker_AuthorityChangeOp& Op);

	void OnComponentUpdate(Worker_ComponentUpdateOp& Op);
	void OnCommandRequest(Worker_CommandRequestOp& Op);
	void OnCommandResponse(Worker_CommandResponseOp& Op);

	void OnReserveEntityIdResponse(Worker_ReserveEntityIdResponseOp& Op);
	void OnCreateEntityIdResponse(Worker_CreateEntityResponseOp& Op);
	void AddPendingActorRequest(Worker_RequestId RequestId, USpatialActorChannel* Channel);

	void CleanupDeletedEntity(Worker_EntityId EntityId);

	void ProcessQueuedResolvedObjects();
	void ResolvePendingOperations(UObject* Object, const UnrealObjectRef& ObjectRef);

private:
	void EnterCriticalSection();
	void LeaveCriticalSection();

	void CreateActor(Worker_EntityId EntityId);
	void RemoveActor(Worker_EntityId EntityId);
	AActor* SpawnNewEntity(SpatialPosition* Position, struct SpatialRotation* Rotation, UClass* ActorClass, bool bDeferred);
	UClass* GetNativeEntityClass(SpatialMetadata* Metadata);

	void HandleActorAuthority(Worker_AuthorityChangeOp& Op);

	void ApplyComponentData(Worker_EntityId EntityId, Worker_ComponentData& Data, USpatialActorChannel* Channel);
	void ApplyComponentUpdate(const Worker_ComponentUpdate& ComponentUpdate, UObject* TargetObject, USpatialActorChannel* Channel, bool bIsHandover);

	void ReceiveRPCCommandRequest(const Worker_CommandRequest& CommandRequest, UObject* TargetObject, UFunction* Function);
	void ReceiveMulticastUpdate(const Worker_ComponentUpdate& ComponentUpdate, UObject* TargetObject, const TArray<UFunction*>& RPCArray);
	void ApplyRPC(UObject* TargetObject, UFunction* Function, TArray<uint8>& PayloadData, int64 CountBits);

	void QueueIncomingRepUpdates(FChannelObjectPair ChannelObjectPair, const FObjectReferencesMap& ObjectReferencesMap, const TSet<UnrealObjectRef>& UnresolvedRefs);
	void QueueIncomingRPC(const TSet<UnrealObjectRef>& UnresolvedRefs, UObject* TargetObject, UFunction* Function, const TArray<uint8>& PayloadData, int64 CountBits);

	void ResolvePendingOperations_Internal(UObject* Object, const UnrealObjectRef& ObjectRef);
	void ResolveIncomingOperations(UObject* Object, const UnrealObjectRef& ObjectRef);
	void ResolveIncomingRPCs(UObject* Object, const UnrealObjectRef& ObjectRef);
	void ResolveObjectReferences(FRepLayout& RepLayout, UObject* ReplicatedObject, FObjectReferencesMap& ObjectReferencesMap, uint8* RESTRICT StoredData, uint8* RESTRICT Data, int32 MaxAbsOffset, TArray<UProperty*>& RepNotifies, bool& bOutSomeObjectsWereMapped, bool& bOutStillHasUnresolved);

	UObject* GetTargetObjectFromChannelAndClass(USpatialActorChannel* Channel, UClass* Class);

	USpatialActorChannel* PopPendingActorRequest(Worker_RequestId RequestId);

private:
	template <typename T>
	friend T* GetComponentData(USpatialReceiver& Receiver, Worker_EntityId EntityId);

	USpatialNetDriver* NetDriver;
	USpatialView* View;
	USpatialSender* Sender;
	USpatialPackageMapClient* PackageMap;
	UWorld* World;
	USpatialTypebindingManager* TypebindingManager;
	UGlobalStateManager* GlobalStateManager;

	// TODO: Figure out how to remove entries when Channel/Actor gets deleted
	TMap<UnrealObjectRef, TSet<FChannelObjectPair>> IncomingRefsMap;
	TMap<FChannelObjectPair, FObjectReferencesMap> UnresolvedRefsMap;
	TArray<TPair<UObject*, UnrealObjectRef>> ResolvedObjectQueue;

	TMap<UnrealObjectRef, FIncomingRPCArray> IncomingRPCMap;

	bool bInCriticalSection;
	TArray<Worker_EntityId> PendingAddEntities;
	TArray<PendingAddComponentWrapper> PendingAddComponents;
	TArray<Worker_EntityId> PendingRemoveEntities;

	TMap<Worker_RequestId, USpatialActorChannel*> PendingActorRequests;
};
