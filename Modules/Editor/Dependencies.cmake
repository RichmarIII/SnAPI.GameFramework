# Editor module dependency wiring.

function(SnAPIGameFrameworkApplyEditorDependencies TargetName)
    if (NOT TARGET ${TargetName})
        message(FATAL_ERROR "SnAPIGameFrameworkApplyEditorDependencies expected target ${TargetName} to exist.")
    endif()

    target_link_libraries(${TargetName}
        PRIVATE
            SnAPI.GameFramework
    )

    snapi_gf_link_renderer_new_static_group(${TargetName})
endfunction()
