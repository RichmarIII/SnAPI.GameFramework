# Runtime module dependency wiring.

function(SnAPIGameFrameworkApplyRuntimeDependencies TargetName)
    if (NOT TARGET ${TargetName})
        message(FATAL_ERROR "SnAPIGameFrameworkApplyRuntimeDependencies expected target ${TargetName} to exist.")
    endif()

    target_link_libraries(${TargetName}
        PRIVATE
            SnAPI.GameFramework
    )

    snapi_gf_link_renderer_new_static_group(${TargetName})
endfunction()
