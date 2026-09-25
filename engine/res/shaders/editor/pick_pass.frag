#version 450 core

layout(location = 0) flat in uint v_NodeId;
layout(location = 0) out uint o_NodeId;

void main()
{
    o_NodeId = v_NodeId;
}
