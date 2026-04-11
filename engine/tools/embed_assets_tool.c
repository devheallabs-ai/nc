/*
 * embed_assets_tool.c
 *
 * Compiles static JS/CSS frontend assets into a C header `nc_ui_assets.h`
 * so they can be bundled into the main NC engine binary without external dependencies.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void embed_file(FILE *out, const char *path, const char *var_name) {
    printf("Embedding %s as %s...\n", path, var_name);
    FILE *in = fopen(path, "rb");
    if (!in) {
        fprintf(stderr, "Error: Cannot open asset file %s\n", path);
        exit(1);
    }
    
    fprintf(out, "static const unsigned char %s[] = {\n    ", var_name);
    int c;
    int count = 0;
    while ((c = fgetc(in)) != EOF) {
        fprintf(out, "0x%02x, ", c);
        count++;
        if (count % 12 == 0) fprintf(out, "\n    ");
    }
    fprintf(out, "0x00\n};\n");
    fprintf(out, "static const unsigned int %s_len = %d;\n\n", var_name, count);
    fclose(in);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: embed_assets_tool <output_header>\n");
        return 1;
    }
    
    const char *out_path = argv[1];
    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Error: Cannot write to %s\n", out_path);
        return 1;
    }
    
    fprintf(out, "/* AUTO-GENERATED FILE. DO NOT EDIT. */\n");
    fprintf(out, "#ifndef NC_UI_ASSETS_H\n");
    fprintf(out, "#define NC_UI_ASSETS_H\n\n");
    
    // Relative to the engine directory
    embed_file(out, "../../nc-ui/runtime.js", "ncui_runtime_js");
    embed_file(out, "../../nc-ui/nc-ui.js", "ncui_core_js");
    embed_file(out, "../../nc-ui/security.js", "ncui_security_js");
    
    fprintf(out, "#endif /* NC_UI_ASSETS_H */\n");
    fclose(out);
    
    printf("Successfully embedded UI assets into %s\n", out_path);
    return 0;
}
