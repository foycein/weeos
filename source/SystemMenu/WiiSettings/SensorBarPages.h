#pragma once

#include "SystemMenu/WiiSettings/PageGroup.h"
#include "SystemMenu/WiiSettings/PageNavigator.h"

#include "Settings/SystemSettings.h"

namespace SystemMenu
{

// "Sensor Bar": where the sensor bar stands, and how sensitive the pointer is.
// The position and the sensitivity act at once, so that the pointer shows the choice; Back puts the old values back.
class SensorBarPages : public PageGroup
{
public:
    // The navigator and the settings must outlive the pages.
    SensorBarPages(PageNavigator& pageNavigator, Settings::SystemSettings& systemSettings);

    bool Owns(PageId page) const override;
    void EnterPage(PageId page) override;
    void BuildPage(PageId page, Page& content) override;

private:
    void BuildSensorBarIndex(Page& content);
    void BuildPositionSet(Page& content);
    void BuildSensitivityIntroduction(Page& content);
    void BuildSensitivitySet(Page& content);

    void ChoosePosition(Settings::SensorBarPosition position);
    void ChangeSensitivity(int change);

    PageNavigator& navigator;
    Settings::SystemSettings& settings;

    Settings::SensorBarPosition positionAtEntry = Settings::SensorBarPosition::BelowTelevision;
    int sensitivityAtEntry = 0;
};

}
