function init() self.elapsed=0 end
function update(dt)
    self.elapsed=self.elapsed+dt
    local path=paths[self.id]
    local travel=path.distance/path.speed
    local phase=self.elapsed%(2*(travel+path.hold))
    local t=0
    if phase<path.hold then t=0
    elseif phase<path.hold+travel then t=(phase-path.hold)/travel
    elseif phase<2*path.hold+travel then t=1
    else t=1-(phase-2*path.hold-travel)/travel end
    local p={}
    for i=1,3 do p[i]=path.start[i]+(path.finish[i]-path.start[i])*t end
    self:set('azure.transform','translation',p)
end
