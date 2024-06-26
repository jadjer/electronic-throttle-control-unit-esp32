// Copyright 2024 Pavel Suprunov
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Created by jadjer on 28.09.23.
//

#pragma once

#include <memory>
#include <functional>

#include "gpio/PinLevel.hpp"
#include "gpio/interface/IInputPin.hpp"
#include "executor/Node.hpp"

enum ModeButtonState {
  MODE_BUTTON_STATE_UNKNOWN = -1,
  MODE_BUTTON_STATE_MODE_1 = 1,
  MODE_BUTTON_STATE_MODE_2 = 2,
  MODE_BUTTON_STATE_MODE_3 = 3
};

using PinLevel = gpio::PinLevel;
using ModeButtonChangeStateCallbackFunction = std::function<void(ModeButtonState)>;

class ModeButton : public executor::Node {
public:
  explicit ModeButton(uint8_t numberOfModeButton1Pin = 7, uint8_t numberOfModeButton2Pin = 6);
  ~ModeButton() override = default;

public:
  void registerChangeValueCallback(ModeButtonChangeStateCallbackFunction const &changeStateCallbackFunction);

private:
  void process() override;

private:
  ModeButtonChangeStateCallbackFunction m_changeStateCallbackFunction = nullptr;

private:
  ModeButtonState m_modeButtonState;
  IInputPinPtr<PinLevel> m_modeButton1;
  IInputPinPtr<PinLevel> m_modeButton2;
};
