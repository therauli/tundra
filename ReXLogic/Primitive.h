// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Primitive_h
#define incl_Primitive_h

#include "NetworkEvents.h"
#include "ResourceInterface.h"

namespace RexLogic
{
    class RexLogicModule;

    class Primitive
    {
     public:
        Primitive(RexLogicModule *rexlogicmodule);
        ~Primitive();
        
        bool HandleOSNE_ObjectUpdate(OpenSimProtocol::NetworkEventInboundData* data);
        bool HandleOSNE_KillObject(uint32_t objectid); 
        bool HandleOSNE_ObjectProperties(OpenSimProtocol::NetworkEventInboundData* data);
                
        bool HandleRexGM_RexMediaUrl(OpenSimProtocol::NetworkEventInboundData* data);
        bool HandleRexGM_RexPrimData(OpenSimProtocol::NetworkEventInboundData* data);
        
        void HandleTerseObjectUpdateForPrim_60bytes(const uint8_t* bytes);
                        
        bool HandleResourceEvent(Core::event_id_t event_id, Foundation::EventDataInterface* data);
             
    private:
        typedef std::map<Core::request_tag_t, Core::entity_id_t> EntityResourceRequestMap; 
            
        RexLogicModule *rexlogicmodule_;
    
        //! @return The entity corresponding to given id AND uuid. This entity is guaranteed to have an existing EC_OpenSimPrim component.
        //!         Does not return null. If the entity doesn't exist, an entity with the given entityid and fullid is created and returned.
        Scene::EntityPtr GetOrCreatePrimEntity(Core::entity_id_t entityid, const RexUUID &fullid);
        Scene::EntityPtr CreateNewPrimEntity(Core::entity_id_t entityid);
        
        //! handle rexprimdata blob coming from server in a genericmessage
        void HandleRexPrimDataBlob(Core::entity_id_t entityid, const uint8_t* primdata);
        
        //! handles changes in drawtype. sets/removes mesh as necessary
        void HandleDrawType(Core::entity_id_t entityid);   
        
        //! handles mesh resource being ready
        void HandleMeshReady(Core::entity_id_t entity, Foundation::ResourcePtr resource);

        //! handles mesh texture resource being ready
        void HandleMeshTextureReady(Core::entity_id_t entity, Foundation::ResourcePtr resource);
        
        //! updates mesh textures if mesh & textures exist, optionally makes requests for the textures
        void HandleMeshTextures(Core::entity_id_t entityid, bool make_new_requests = false);
                
        //! discards request tags for certain entity
        void DiscardRequestTags(Core::entity_id_t, EntityResourceRequestMap& map);
        
        //! resource request tags for meshes
        std::map<Core::request_tag_t, Core::entity_id_t> mesh_request_tags_;    
     
        //! resource request tags for mesh textures
        std::map<Core::request_tag_t, Core::entity_id_t> mesh_texture_request_tags_; 
    };
}
#endif