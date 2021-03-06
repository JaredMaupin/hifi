//
//  Created by Sam Gondelman 7/11/2017
//  Copyright 2017 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//
#include "StaticRayPick.h"

StaticRayPick::StaticRayPick(const glm::vec3& position, const glm::vec3& direction, const PickFilter& filter, float maxDistance, bool enabled) :
    RayPick(filter, maxDistance, enabled),
    _pickRay(position, direction)
{
}

PickRay StaticRayPick::getMathematicalPick() const {
    return _pickRay;
}