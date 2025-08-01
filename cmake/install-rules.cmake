install(
    TARGETS swtor_combat_explorer_exe
    RUNTIME COMPONENT swtor_combat_explorer_Runtime
)

if(PROJECT_IS_TOP_LEVEL)
  include(CPack)
endif()
