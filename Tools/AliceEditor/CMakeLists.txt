# The document model stays testable on every host; native widgets remain a Windows tool.
alice_module(Alice.Editor.Model
    FOLDER "Tools"
    SOURCES DocumentModel.h DocumentModel.cpp AtomicSave.cpp
    DEPS Alice.Verbs)
target_include_directories(Alice.Editor.Model PUBLIC "${PROJECT_SOURCE_DIR}/Tools")
if(WIN32)
    alice_executable(Alice.Editor FOLDER "Tools"
        SOURCES Win32Editor.cpp SceneViewport.cpp SceneViewport.h
        DEPS Alice.Editor.Model user32 gdi32 comdlg32 shell32 ole32)
    set_target_properties(Alice.Editor PROPERTIES WIN32_EXECUTABLE TRUE)
endif()
