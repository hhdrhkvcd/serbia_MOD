require ecs
require math
require DagorMath

// Режим «Превосходство» [A / B / C] и убывание очков подкреплений команд (2500 -> 0)
[es(tag=server, no_order)]
def update_conquest_zones_es(info : UpdateStageInfoAct;
                             transform : float3x4;
                             zone__radius : float;
                             var zone__ownerTeam : int&;
                             var zone__captureProgress : float&)
  let zonePos = transform[3]
  var team1Count = 0
  var team2Count = 0

  query() <| $ [es] (transform aka tank_tm : float3x4; tank__team : int; tank__hp : float)
    if tank__hp <= 0.0
      return
    if distance(float2(tank_tm[3].x, tank_tm[3].z), float2(zonePos.x, zonePos.z)) <= zone__radius
      if tank__team == 1
        team1Count += 1
      elif tank__team == 2
        team2Count += 1

  if team1Count > 0 && team2Count == 0
    zone__captureProgress = min(100.0, zone__captureProgress + info.dt * 12.0)
    if zone__captureProgress >= 100.0
      zone__ownerTeam = 1
  elif team2Count > 0 && team1Count == 0
    zone__captureProgress = max(-100.0, zone__captureProgress - info.dt * 12.0)
    if zone__captureProgress <= -100.0
      zone__ownerTeam = 2
