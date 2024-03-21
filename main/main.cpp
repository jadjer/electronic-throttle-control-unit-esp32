// Copyright 2023 Pavel Suprunov
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

#include <cmath>
#include <vector>
#include <iostream>

#include "executor/Executor.hpp"
#include "motor/Motor.hpp"
#include "accelerator/Scaler.hpp"
#include "accelerator/Formatter.hpp"
#include "accelerator/Accelerator.hpp"
#include "indicator/BlinkIndicator.hpp"
#include "ECU/HondaECU.hpp"
#include "ECU/UartNetworkConnector.hpp"
#include "ECU/KLineNetworkConnector.hpp"

#include "Drv8825.hpp"
#include "ModeButton.hpp"
#include "Controller.hpp"
#include "SetupButton.hpp"

template<typename T>
requires std::is_arithmetic_v<T> T averageFilter(T const value, float const threshold)
{
    static size_t const windowSize = 3;
    static std::vector<T> accelerationItems;

    accelerationItems.reserve(windowSize);

    for (size_t i = 0; i < windowSize - 1; i++)
    {
        accelerationItems[i] = accelerationItems[i + 1];
    }

    accelerationItems.at(windowSize - 1) = value;

    T result = 0;

    for (auto const accelerationItem : accelerationItems)
    {
        result += accelerationItem;
    }

    result /= windowSize;

    if (std::fabs(value - result) > threshold)
    {
        return result;
    }

    return value;
}

constexpr uint32_t const motorDefaultSpeed = (32 * 200 * 1000) / 60;
constexpr uint32_t const motorDefaultAcceleration = motorDefaultSpeed;
constexpr uint32_t const motorDefaultDeceleration = motorDefaultSpeed * 2;

constexpr uint32_t const acceleratorMinimalValue = 840;
constexpr uint32_t const acceleratorMaximalValue = 2570;
constexpr uint8_t const throttlePositionMinimalValue = 0;
constexpr uint8_t const throttlePositionMaximalValue = 100;

extern "C" void app_main(void)
{
    auto indicatorPtr = std::make_shared<indicator::BlinkIndicator>(2);

    auto motorDriverPtr = std::make_unique<DRV8825>();
    motorDriverPtr->setMicroSteps(motor::driver::MOTOR_32_MICRO_STEPS);

    auto motorPtr = std::make_shared<motor::Motor>(std::move(motorDriverPtr));
    motorPtr->setFrequency(1000000);
    motorPtr->setSpeedInStepsPerSecond(motorDefaultSpeed);
    motorPtr->setAccelerationInStepsPerSecondPerSecond(motorDefaultAcceleration);
    motorPtr->setDecelerationInStepsPerSecondPerSecond(motorDefaultDeceleration);

    auto controllerPtr = std::make_shared<Controller>();
    controllerPtr->registerChangeValueCallback(
        [&](uint32_t const acceleratorValue)
        {
            auto steps = static_cast<int32_t>(acceleratorValue * 115);
            std::cout << "| Motor: " << steps << " ";

            motorPtr->setTargetPositionInSteps(steps);
        });

    auto scalerPtr = std::make_shared<accelerator::Scaler>(throttlePositionMinimalValue, throttlePositionMaximalValue);
    scalerPtr->registerChangeValueCallback(
        [&](uint32_t const scaledValue)
        {
            std::cout << "| Scaled value: " << scaledValue << " ";
            controllerPtr->setAcceleration(scaledValue);
        });

    auto formatterPtr = std::make_shared<accelerator::Formatter>(
        acceleratorMinimalValue, acceleratorMaximalValue, throttlePositionMinimalValue, throttlePositionMaximalValue);
    formatterPtr->registerChangeValueCallback(
        [&](uint32_t const formattedValue)
        {
            std::cout << "| Formatted value: " << formattedValue << " ";
            scalerPtr->setValue(formattedValue);
        });

    auto acceleratorPtr = std::make_shared<accelerator::Accelerator>();
    acceleratorPtr->setFrequency(8);
    acceleratorPtr->registerChangeAccelerateCallback(
        [&](uint32_t acceleratorValue_InPercentage)
        {
            std::cout << "Accelerator: " << acceleratorValue_InPercentage << "mV ";

            acceleratorValue_InPercentage = static_cast<uint32_t>(static_cast<float>(acceleratorValue_InPercentage) * 0.1) * 10;
            std::cout << "| Accelerator(filter1): " << acceleratorValue_InPercentage << " ";

            acceleratorValue_InPercentage = averageFilter(acceleratorValue_InPercentage, 20);
            std::cout << "| Accelerator(filter2): " << acceleratorValue_InPercentage << " ";

            formatterPtr->setValue(acceleratorValue_InPercentage);
            std::cout << std::endl;
        });

    auto setupButtonPtr = std::make_shared<SetupButton>();
    setupButtonPtr->registerChangeValueCallback(
        [&](SetupButtonState const setupButtonState)
        {
            if (setupButtonState == SETUP_BUTTON_HELD)
            {
                scalerPtr->enable();
                controllerPtr->enable();
            }
            if (setupButtonState == SETUP_BUTTON_PRESSED)
            {
                scalerPtr->disable();
                controllerPtr->disable();
            }
        });

    auto modeButtonPtr = std::make_shared<ModeButton>();
    modeButtonPtr->registerChangeValueCallback(
        [&](ModeButtonState const modeButtonState)
        {
            float accelerationRate;

            switch (modeButtonState)
            {
            case MODE_BUTTON_STATE_MODE_1:
            {
                accelerationRate = 2.0;
                std::cout << std::endl << "Motor mode 1" << std::endl;

                break;
            }
            case MODE_BUTTON_STATE_MODE_2:
            {
                accelerationRate = 1.0;
                std::cout << std::endl << "Motor mode 2" << std::endl;

                break;
            }
            default:
            case MODE_BUTTON_STATE_MODE_3:
            {
                accelerationRate = 0.5;
                std::cout << std::endl << "Motor mode 3" << std::endl;

                break;
            }
            }

            auto acceleration = motorDefaultAcceleration * accelerationRate;

            motorPtr->setAccelerationInStepsPerSecondPerSecond(acceleration);
            motorPtr->setDecelerationInStepsPerSecondPerSecond(acceleration);
        });

    auto uart = std::make_unique<ECU::UartNetworkConnector>(3, 1, 2);
    auto kLine = std::make_unique<ECU::KLineNetworkConnector>(1, std::move(uart));
    auto ecu = std::make_shared<ECU::HondaECU>(std::move(kLine));

    auto executorPtr = std::make_unique<executor::Executor>();
    executorPtr->addNode(ecu);
    executorPtr->spin();
}
