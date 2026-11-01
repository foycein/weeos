target_sources(Ios PRIVATE
    Ipv4Address.cpp
    NetworkConfigService.cpp
    NetworkConfiguration.cpp
    NetworkConfigurationCheck.cpp
    PcConnection.cpp
    WirelessMacAddress.cpp
    WirelessService.cpp
)
target_link_libraries(Ios PUBLIC iphlpapi)
