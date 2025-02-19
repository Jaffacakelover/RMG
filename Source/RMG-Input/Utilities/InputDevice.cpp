/*
 * Rosalie's Mupen GUI - https://github.com/Rosalie241/RMG
 *  Copyright (C) 2020 Rosalie Wanders <rosalie@mailbox.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
#include "InputDevice.hpp"

using namespace Utilities;

InputDevice::InputDevice()
{

}

InputDevice::~InputDevice()
{
    this->CloseDevice();
}

void InputDevice::SetSDLThread(Thread::SDLThread* sdlThread)
{
    this->sdlThread = sdlThread;
    connect(this->sdlThread, &Thread::SDLThread::OnInputDeviceFound, this,
        &InputDevice::on_SDLThread_DeviceFound);
    connect(this->sdlThread, &Thread::SDLThread::OnDeviceSearchFinished, this,
        &InputDevice::on_SDLThread_DeviceSearchFinished);
}

SDL_Joystick* InputDevice::GetJoystickHandle()
{
    return this->joystick.loadAcquire();
}

SDL_GameController* InputDevice::GetGameControllerHandle()
{
    return this->gameController.loadAcquire();
}

bool InputDevice::StartRumble(void)
{
    if (this->gameController.loadAcquire() != nullptr)
    {
        return SDL_GameControllerRumble(this->gameController.loadAcquire(), 0xFFFF, 0xFFFF, SDL_HAPTIC_INFINITY) == 0;
    }
    else if (this->joystick.loadAcquire() != nullptr)
    {
        return SDL_JoystickRumble(this->joystick.loadAcquire(), 0xFFFF, 0xFFFF, SDL_HAPTIC_INFINITY) == 0;
    }

    return false;
}

bool InputDevice::StopRumble(void)
{
    if (this->gameController.loadAcquire() != nullptr)
    {
        return SDL_GameControllerRumble(this->gameController.loadAcquire(), 0, 0, 0) == 0;
    }
    else if (this->joystick.loadAcquire() != nullptr)
    {
        return SDL_JoystickRumble(this->joystick.loadAcquire(), 0, 0, 0) == 0;
    }

    return false;
}

bool InputDevice::HasOpenDevice()
{
    return this->hasOpenDevice;
}

void InputDevice::OpenDevice(std::string name, std::string path, std::string serial, int num)
{
    // wait until SDLThread is done first
    while (this->sdlThread->GetCurrentAction() != SDLThreadAction::None)
    {
        QThread::msleep(5);
    }

    this->foundDevicesWithNameMatch.clear();
    this->desiredDevice = {name, path, serial, num};
    this->isOpeningDevice = true;

    // tell SDLThread to query input devices
    this->sdlThread->SetAction(SDLThreadAction::GetInputDevices);
}

bool InputDevice::IsOpeningDevice(void)
{
    return this->isOpeningDevice;
}

bool InputDevice::CloseDevice()
{
    if (this->joystick.loadAcquire() != nullptr)
    {
        SDL_JoystickClose(this->joystick.loadAcquire());
        this->joystick.storeRelease(nullptr);
    }

    if (this->gameController.loadAcquire() != nullptr)
    {
        SDL_GameControllerClose(this->gameController.loadAcquire());
        this->gameController.storeRelease(nullptr);
    }

    return true;
}

void InputDevice::on_SDLThread_DeviceFound(QString name, QString path, QString serial, int number)
{
    if ((!this->isOpeningDevice) || 
        (this->desiredDevice.name != name.toStdString()))
    {
        return;
    }

    SDLDevice device;
    device.name = name.toStdString();
    device.path = path.toStdString();
    device.serial = serial.toStdString();
    device.number = number;

    this->foundDevicesWithNameMatch.push_back(device);
}

void InputDevice::on_SDLThread_DeviceSearchFinished(void)
{
    if (!this->isOpeningDevice)
    {
        return;
    }

    this->CloseDevice();

    if (this->foundDevicesWithNameMatch.empty())
    {
        this->isOpeningDevice = false;
        this->hasOpenDevice = false;
        return;
    }

    // try to find exact match
    SDLDevice device;
    bool hasDevice = false;

    auto iter = std::find(this->foundDevicesWithNameMatch.begin(), this->foundDevicesWithNameMatch.end(), this->desiredDevice);
    if (iter != this->foundDevicesWithNameMatch.end())
    { // use exact match
        device.name = iter->name;
        device.path = iter->path;
        device.serial = iter->serial;
        device.number = iter->number;
    }
    else
    { // no exact match, try to find name + serial match first
        if (!this->desiredDevice.serial.empty())
        { // only try serial match when it's not empty
            for (const auto& deviceNameMatch : this->foundDevicesWithNameMatch)
            {
                if (deviceNameMatch.serial == this->desiredDevice.serial)
                {
                    device = deviceNameMatch;
                    hasDevice = true;
                    break;
                }
            }
        }

        if (!hasDevice)
        { // fallback to name only match
            device = this->foundDevicesWithNameMatch.at(0);
        }
    }


    if (SDL_IsGameController(device.number))
    {
        this->gameController.storeRelease(SDL_GameControllerOpen(device.number));
    }
    else
    {
        this->joystick.storeRelease(SDL_JoystickOpen(device.number));
    }  

    this->isOpeningDevice = false;
    this->hasOpenDevice = this->joystick != nullptr || this->gameController != nullptr;
}

bool InputDevice::on_SDL_DeviceAdded(SDL_GameController* controller, std::string name, std::string path, std::string serial, int number)
{
    if (this->hasOpenDevice || this->isOpeningDevice)
    {
        return false;
    }

    SDLDevice device = {name, path, serial, 0};
    SDLDevice desiredDevice = this->desiredDevice;
    desiredDevice.number = 0;

    bool hasDevice = false;

    if (device == desiredDevice)
    { // use exact match
        printf("on_SDL_DeviceAdded exact match!\n");
        this->gameController.storeRelease(controller);
        // TODO: further validation?
        this->joystick.storeRelease(nullptr);
        this->hasOpenDevice = true;
        this->isOpeningDevice = false;
        this->currentDeviceNum = number;
        return true;
    }
    else
    { // no exact match, try to find name + serial match first
        if (!desiredDevice.serial.empty())
        { // only try serial match when it's not empty
            if (device.name == desiredDevice.name &&
                device.serial == desiredDevice.serial)
            {
                hasDevice = true;
            }
        }

        if (device.name == desiredDevice.name)
        { // fallback to name only match
            hasDevice = true;
        }

        if (hasDevice)
        {
            printf("on_SDL_DeviceAdded partial match\n");

            this->gameController.storeRelease(controller);
            this->joystick.storeRelease(nullptr);
            this->hasOpenDevice = true;
            this->isOpeningDevice = false;
            return true;
        }
    }

    return false;
}

bool InputDevice::on_SDL_DeviceRemoved(int number)
{
    if (number == this->currentDeviceNum)
    {
        this->CloseDevice();
        this->hasOpenDevice   = false;
        this->isOpeningDevice = false;
        printf("device removed from input device!\n");
        return true;
    }

    return false;
}
