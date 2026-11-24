#pragma once
// Included after tiny_gltf.h. Normalize sparse streams before any consumer reads them.
namespace {
void normalizeGltfAccessors(tinygltf::Model& model) {
    constexpr std::size_t budget=512U*1024U*1024U;
    std::size_t expanded=0;
    const auto span=[&](int viewIndex,std::size_t offset,std::size_t count,std::size_t stride,std::size_t element){
        if(viewIndex<0||static_cast<std::size_t>(viewIndex)>=model.bufferViews.size())throw std::invalid_argument("Accessor references missing buffer view");
        const auto& view=model.bufferViews[viewIndex];
        if(view.buffer<0||static_cast<std::size_t>(view.buffer)>=model.buffers.size())throw std::invalid_argument("Buffer view references missing buffer");
        const auto& bytes=model.buffers[view.buffer].data;
        if(view.byteOffset>bytes.size()||view.byteLength>bytes.size()-view.byteOffset||offset>view.byteLength)
            throw std::invalid_argument("Buffer view exceeds its buffer");
        if(count&&(element>view.byteLength-offset||stride<element||(count-1)>(view.byteLength-offset-element)/stride))
            throw std::invalid_argument("Accessor exceeds its buffer view");
        return bytes.data()+view.byteOffset+offset;
    };
    for(auto& a:model.accessors){
        const int components=tinygltf::GetNumComponentsInType(static_cast<std::uint32_t>(a.type));
        const int component=tinygltf::GetComponentSizeInBytes(static_cast<std::uint32_t>(a.componentType));
        if(components<=0||component<=0)throw std::invalid_argument("Unknown accessor element type");
        const auto element=static_cast<std::size_t>(components*component);
        if(a.count>budget/element)throw std::invalid_argument("Accessor exceeds decoded size budget");
        const unsigned char* source=nullptr;std::size_t stride=element;
        if(a.bufferView>=0){
            const auto& view=model.bufferViews.at(a.bufferView);const int declared=a.ByteStride(view);
            if(declared<=0)throw std::invalid_argument("Invalid accessor stride");stride=static_cast<std::size_t>(declared);
            source=span(a.bufferView,a.byteOffset,a.count,stride,element);
        }else if(a.byteOffset)throw std::invalid_argument("Bufferless accessor must have zero offset");
        if(!a.sparse.isSparse&&source)continue;
        const auto size=a.count*element;if(size>budget-expanded)throw std::invalid_argument("Sparse expansion exceeds asset budget");expanded+=size;
        tinygltf::Buffer buffer;buffer.data.resize(size);
        if(source)for(std::size_t i=0;i<a.count;++i)std::memcpy(buffer.data.data()+i*element,source+i*stride,element);
        if(a.sparse.isSparse){
            if(a.sparse.count<0||static_cast<std::size_t>(a.sparse.count)>a.count)throw std::invalid_argument("Invalid sparse count");
            const int type=a.sparse.indices.componentType;
            if(type!=5121&&type!=5123&&type!=5125)throw std::invalid_argument("Invalid sparse index type");
            const auto indexBytes=static_cast<std::size_t>(tinygltf::GetComponentSizeInBytes(static_cast<std::uint32_t>(type)));
            const auto count=static_cast<std::size_t>(a.sparse.count);
            const auto* indices=span(a.sparse.indices.bufferView,a.sparse.indices.byteOffset,count,indexBytes,indexBytes);
            const auto* values=span(a.sparse.values.bufferView,a.sparse.values.byteOffset,count,element,element);
            std::uint32_t previous=0;
            for(std::size_t i=0;i<count;++i){
                std::uint32_t index=0;std::memcpy(&index,indices+i*indexBytes,indexBytes);
                if(index>=a.count||(i&&index<=previous))throw std::invalid_argument("Sparse indices must be increasing and in range");
                previous=index;std::memcpy(buffer.data.data()+index*element,values+i*element,element);
            }
        }
        tinygltf::BufferView view;view.buffer=static_cast<int>(model.buffers.size());view.byteLength=size;
        a.bufferView=static_cast<int>(model.bufferViews.size());a.byteOffset=0;a.sparse.isSparse=false;
        model.buffers.push_back(std::move(buffer));model.bufferViews.push_back(view);
    }
}
}
