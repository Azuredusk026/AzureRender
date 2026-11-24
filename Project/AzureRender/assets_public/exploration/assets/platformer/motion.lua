local paths={["platform-3"]={["start"]={-0.65,-0.19999999999999998,-19.79132685661316},["finish"]={0.65,-0.19999999999999998,-19.79132685661316},["speed"]=0.3,["hold"]=1.5,["distance"]=1.3,["period"]=11.666666666666668},["platform-5"]={["start"]={0,0.0,-33.15932173728943},["finish"]={0,0.35,-33.15932173728943},["speed"]=0.18,["hold"]=1.5,["distance"]=0.35,["period"]=6.888888888888889}}
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
