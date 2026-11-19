cmake_minimum_required(VERSION 3.16)

project(ResistorDivider VERSION 2.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets OpenGL OpenGLWidgets)

qt_add_executable(ResistorDivider
    main.cpp
    MainWindow.h
    MainWindow.cpp
    Ui.h
    Ui.cpp
    Format.h

    # Resistor divider
    DividerPage.h
    DividerPage.cpp
    CircuitWidget.h
    CircuitWidget.cpp
    ResistorCalculator.h
    ResistorCalculator.cpp

    # RC filter
    RcFilterPage.h
    RcFilterPage.cpp
    RcCircuitWidget.h
    RcCircuitWidget.cpp
    FrequencyResponseWidget.h
    FrequencyResponseWidget.cpp
    RcFilterCalculator.h
    RcFilterCalculator.cpp

    # Circuit simulator
    simulator/SimulatorPage.h
    simulator/SimulatorPage.cpp
    simulator/CircuitView.h
    simulator/CircuitView.cpp
    simulator/ScopePanel.h
    simulator/ScopePanel.cpp
    simulator/ElementDialog.h
    simulator/ElementDialog.cpp
    simulator/ElementPainter.h
    simulator/ElementPainter.cpp
    simulator/ExampleCircuits.h
    simulator/ExampleCircuits.cpp
    simulator/Circuit.h
    simulator/Circuit.cpp
    simulator/Element.h
    simulator/Element.cpp
    simulator/Elements.h
    simulator/ElementsPassive.cpp
    simulator/ElementsSources.cpp
    simulator/ElementsActive.cpp

    # MOSFET 3D: physics (no Qt)
    mosfet/physics/MosfetParameters.h
    mosfet/physics/Electrostatics.h
    mosfet/physics/DeviceState.h
    mosfet/physics/DeviceModel.h
    mosfet/physics/AnalyticalModel.cpp
    mosfet/physics/Validation.h
    mosfet/physics/Validation.cpp
    # MOSFET 3D: parameter registry (no Qt)
    mosfet/config/Parameters.h
    mosfet/config/Parameters.cpp
    # MOSFET 3D: geometry and particles (no Qt)
    mosfet/geometry/Region.h
    mosfet/geometry/Region.cpp
    mosfet/geometry/MosfetGeometry.h
    mosfet/geometry/MosfetGeometry.cpp
    mosfet/particles/CarrierParticles.h
    mosfet/particles/CarrierParticles.cpp
    # MOSFET 3D: OpenGL rendering
    mosfet/render/Camera.h
    mosfet/render/Camera.cpp
    mosfet/render/Materials.h
    mosfet/render/MosfetRenderer.h
    mosfet/render/MosfetRenderer.cpp
    # MOSFET 3D: user interface
    mosfet/ui/PartInfo.h
    mosfet/ui/PartInfo.cpp
    mosfet/ui/MosfetView.h
    mosfet/ui/MosfetView.cpp
    mosfet/ui/MosfetPage.h
    mosfet/ui/MosfetPage.cpp
    mosfet/ui/ParamControl.h
    mosfet/ui/ParamControl.cpp
    mosfet/ui/PlotWidget.h
    mosfet/ui/PlotWidget.cpp
    mosfet/ui/GraphPanel.h
    mosfet/ui/GraphPanel.cpp
    mosfet/ui/Explainer.h
    mosfet/ui/Explainer.cpp
    mosfet/ui/Presets.h
    mosfet/ui/Presets.cpp
    mosfet/ui/PhysicsCheck.h
    mosfet/ui/PhysicsCheck.cpp
)

target_include_directories(ResistorDivider PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/simulator
    ${CMAKE_CURRENT_SOURCE_DIR}/mosfet
)

target_link_libraries(ResistorDivider PRIVATE Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)

set_target_properties(ResistorDivider PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
)
