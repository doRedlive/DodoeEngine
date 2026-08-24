local world = dodoe.getWorld()
local ResourceManager = dodoe.ResourceManager
local Time = dodoe.Time

local SpriteRendererComponent = dodoe.SpriteRendererComponent
local Rigidbody2dComponent = dodoe.Rigidbody2dComponent
local BoxCollider2dComponent = dodoe.BoxCollider2dComponent
local TransformComponent = dodoe.TransformComponent
local TagComponent = dodoe.TagComponent

local RigidbodyBodyType = dodoe.RigidbodyBodyType

local Input = dodoe.Input

local TestSystem  = {}

function TestSystem:doStart(reg)
    print("doStart from test.lua")
end

function TestSystem:doUpdate(reg, dt)
    -- print(dt)
end

world:registerSystem(TestSystem)
