// Интерфейс ангара в стиле War Thunder на движке darg / Quirrel (Dagor Engine UI)
from "%darg/ui_imports.nut" import *

let NATIONS = [
  { id = "ru", label = "РОССИЯ" }
  { id = "rs", label = "СЕРБИЯ" }
  { id = "cn", label = "КИТАЙ" }
  { id = "us", label = "США" }
  { id = "eu", label = "НАТО / ЕС" }
  { id = "fr", label = "ФРАНЦИЯ" }
  { id = "se", label = "ШВЕЦИЯ" }
]

let selectedNation = Watched("rs")
let selectedTank = Watched("M-84AS2 Војвода")

function battleButton(onStartOnline) {
  return {
    rendObj = ROBJ_BOX
    size = [hdpx(280), hdpx(58)]
    fillColor = Color(175, 28, 28, 245)
    borderColor = Color(248, 113, 113, 255)
    borderWidth = hdpx(1)
    behavior = Behaviors.Button
    onClick = onStartOnline
    halign = ALIGN_CENTER
    valign = ALIGN_CENTER
    children = {
      rendObj = ROBJ_TEXT
      text = "В БОЙ!"
      fontSize = hdpx(24)
      color = Color(255, 255, 255)
    }
  }
}

function rootHangarUi(onStartOnline) {
  return @() {
    watch = [selectedNation, selectedTank]
    size = flex()
    flow = FLOW_VERTICAL
    padding = hdpx(16)
    children = [
      {
        size = [flex(), SIZE_TO_CONTENT]
        flow = FLOW_HORIZONTAL
        halign = ALIGN_CENTER
        gap = hdpx(10)
        children = [battleButton(onStartOnline)]
      }
      {
        size = [flex(), SIZE_TO_CONTENT]
        flow = FLOW_HORIZONTAL
        halign = ALIGN_CENTER
        gap = hdpx(8)
        margin = [hdpx(10), 0, 0, 0]
        children = NATIONS.map(@(n) {
          rendObj = ROBJ_BOX
          padding = [hdpx(6), hdpx(14)]
          fillColor = selectedNation.value == n.id ? Color(65, 48, 18, 230) : Color(16, 22, 30, 210)
          borderColor = selectedNation.value == n.id ? Color(250, 204, 21, 255) : Color(100, 116, 139, 120)
          borderWidth = hdpx(1)
          behavior = Behaviors.Button
          onClick = @() selectedNation(n.id)
          children = { rendObj = ROBJ_TEXT, text = n.label, color = Color(240, 245, 250) }
        })
      }
    ]
  }
}

return { rootHangarUi }
