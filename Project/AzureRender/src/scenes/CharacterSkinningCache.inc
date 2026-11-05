// Included inside the renderer namespace. Recording threads publish value tickets.
void CharacterSceneRenderer::noteSkinningOutputRecorded(SkinningOutputTicket ticket){
    std::lock_guard<std::mutex> lock(skinningRecordedMutex_);
    recordedSkinningOutputs_.push_back(ticket);
}
void CharacterSceneRenderer::commitSkinningOutputs(std::uint64_t submission)noexcept{
    std::array<bool,kMaxFramesInFlight> seen{},conflict{};
    {
        std::lock_guard<std::mutex> lock(skinningRecordedMutex_);
        for(const auto& ticket:recordedSkinningOutputs_){
            seen[ticket.slot]=true;
            if(ticket.generation!=staticSkinningOutputs_[ticket.slot].generation())conflict[ticket.slot]=true;
        }
        recordedSkinningOutputs_.clear();
    }
    for(std::size_t slot=0;slot<kMaxFramesInFlight;++slot){
        auto& cache=staticSkinningOutputs_[slot];
        if(conflict[slot])cache.invalidate();
        else if(seen[slot])cache.commit(cache.generation(),submission);
    }
}
void CharacterSceneRenderer::abandonSkinningOutputs()noexcept{
    {std::lock_guard<std::mutex> lock(skinningRecordedMutex_);recordedSkinningOutputs_.clear();}
    for(auto& cache:staticSkinningOutputs_)cache.cancel();
}
