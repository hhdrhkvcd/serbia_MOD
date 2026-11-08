require ecs
require net
require math
require DagorMath
require DagorSystem
require DagorDataBlock
require DngNet

// Авторитетная серверная сессия игрока и спавн выбранного танка (daNetGame ECS)
def spawn_player_tank(player_eid : EntityId; veh_template : string; team : int; spawn_pos : float3) : EntityId
  let tankEid = createEntitySync(veh_template) <| $(var init : ComponentsInitializer)
    var tm = IDENT_TM
    tm[3] = spawn_pos
    set(init, "transform", tm)
    set(init, "tank__team", team)
  query(player_eid) <| $ [es] (var possessed : EntityId&)
    possessed = tankEid
  return tankEid

[es(tag=server, on_appear, REQUIRE=player)]
def on_steel_front_player_connected(evt : Event; eid : EntityId; selectedVehicleTemplate : string; team : int)
  let spawnX = team == 1 ? -120.0 : 120.0
  let spawnZ = team == 1 ? 115.0 : -115.0
  spawn_player_tank(eid, selectedVehicleTemplate, team, float3(spawnX, 1.0, spawnZ))
