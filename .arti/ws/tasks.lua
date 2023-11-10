return {
    {
        name = "configure",
        type = "shell",
        command = {
            {
                command = "cmake",
                args = {
                    "-S " .. workspace_folder,
                    "-B " .. ws.build_dir,
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=1",
                    "-DCMAKE_BUILD_TYPE=" .. state.build_mode
                }
            },
            {
                command = "ln",
                args = {
                    "-f",
                    "-s",
                    ws.build_dir .. dirsep .. "compile_commands.json",
                    workspace_folder .. dirsep .. "compile_commands.json"
                }
            }
        },
        cwd = workspace_folder,
        depends = {}
    },
    {
        name = "build",
        type = "shell",
        command = "cmake",
        args = {
            "--build " .. ws.build_dir,
            "--config " .. state.build_mode,
            "-j " .. number_of_cores
        },
        cwd = ws.build_dir,
        depends = {
            "configure"
        },
        default = true,
    }
}
