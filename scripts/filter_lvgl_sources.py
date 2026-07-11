from pathlib import Path


Import("env")

project_dir = Path(env.subst("$PROJECT_DIR"))
blend_dir = (
    project_dir
    / ".pio"
    / "libdeps"
    / env.subst("$PIOENV")
    / "lvgl"
    / "src"
    / "draw"
    / "sw"
    / "blend"
)

for asm_source in blend_dir.glob("**/*.S"):
    asm_source.unlink()
