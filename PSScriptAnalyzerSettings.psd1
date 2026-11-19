# @file PSScriptAnalyzerSettings.psd1
# @brief PowerShell analysis rules for wfc-vbrun-owned scripts (WSP-LANG-0007).
# @note Pinned analyzer: PSScriptAnalyzer 1.25.0.
@{
    Severity = @('Error', 'Warning')
    IncludeRules = @(
        'PSAvoidAssignmentToAutomaticVariable'
        'PSAvoidUsingCmdletAliases'
        'PSAvoidUsingInvokeExpression'
        'PSAvoidUsingPositionalParameters'
        'PSUseDeclaredVarsMoreThanAssignments'
        'PSUseApprovedVerbs'
        'PSUseSingularNouns'
        'PSAvoidGlobalVars'
        'PSUseCmdletCorrectly'
    )
}
