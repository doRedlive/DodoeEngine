
local worldManager = dodoe.WorldManager

local world = worldManager.activeWorld()
local scene = world:activeScene()

local player = scene:createEntity("One")

player:setComponent(TagComponent,{
    tag = "Player"
})

local sr = player:addComponent(SpriteRendererComponent)
sr.texture_path = "engine/"

