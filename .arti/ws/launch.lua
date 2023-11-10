return {
    {
        name = "run",
        program = ws.build_dir .. dirsep .. "src" .. dirsep .. "app" .. dirsep .. "arti-spotify",
        args = {},
        cwd = workspace_folder,
        depends = {
            "build"
        },
        default = true
    }
}
