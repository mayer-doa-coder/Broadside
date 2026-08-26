#pragma once

// Broadside — Phase 2: GLSL load / compile / link.
//
// AGENT.md standing rule: "Check every shader compile and link log and print it."
// A shader that fails silently produces a black screen with no error, which is the
// single most expensive way to lose an afternoon on this project. Every failure
// path below prints the driver's own log, with the file it came from.

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

class Shader {
public:
    GLuint id = 0;

    Shader() = default;
    ~Shader() { destroy(); }

    // Owns a GL program object, so copying would double-delete it.
    Shader(const Shader&)            = delete;
    Shader& operator=(const Shader&) = delete;

    Shader(Shader&& other) noexcept : id(other.id) { other.id = 0; }
    Shader& operator=(Shader&& other) noexcept
    {
        if (this != &other) {
            destroy();
            id       = other.id;
            other.id = 0;
        }
        return *this;
    }

    bool valid() const { return id != 0; }
    void use() const   { glUseProgram(id); }

    // Must be called while the GL context is still alive — i.e. before glfwTerminate.
    void destroy()
    {
        if (id != 0) {
            glDeleteProgram(id);
            id = 0;
        }
    }

    // Compiles both stages and links them. On any failure it prints the reason,
    // leaves the object invalid, and returns false — it never half-succeeds.
    //
    // `common` is GLSL spliced into BOTH stages just after their #version line.
    // Phase 5 uses it for the one shared copy of the illumination model: GLSL has
    // no #include, and two drifting copies of computeLighting would quietly make
    // the Gouraud-vs-Phong comparison unfair (guide 5.2, implementation note).
    bool loadFromFiles(const std::string& vertPath, const std::string& fragPath,
                       const std::string& common = std::string())
    {
        destroy();

        std::string vertSrc, fragSrc;
        if (!readFile(vertPath, vertSrc)) return false;
        if (!readFile(fragPath, fragSrc)) return false;

        if (!common.empty()) {
            vertSrc = spliceAfterVersion(vertSrc, common, vertPath);
            fragSrc = spliceAfterVersion(fragSrc, common, fragPath);
        }

        const GLuint vs = compileStage(GL_VERTEX_SHADER, vertSrc, vertPath);
        if (vs == 0) return false;

        const GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragSrc, fragPath);
        if (fs == 0) {
            glDeleteShader(vs);
            return false;
        }

        const GLuint program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        glLinkProgram(program);

        // Detach and delete the stage objects either way — once linked, the program
        // holds everything it needs and the shader objects are just leaked handles.
        glDetachShader(program, vs);
        glDetachShader(program, fs);
        glDeleteShader(vs);
        glDeleteShader(fs);

        if (!checkLink(program, vertPath, fragPath)) {
            glDeleteProgram(program);
            return false;
        }

        id = program;
        std::printf("[shader] linked program %u  (%s + %s)\n",
                    id, vertPath.c_str(), fragPath.c_str());
        std::fflush(stdout);
        return true;
    }

    // --- uniform setters ---------------------------------------------------
    // glUniform* with location -1 is a silent no-op, so an optimised-out uniform
    // is harmless here rather than a GL error.
    void setInt(const char* name, int value) const
    {
        glUniform1i(glGetUniformLocation(id, name), value);
    }

    void setFloat(const char* name, float value) const
    {
        glUniform1f(glGetUniformLocation(id, name), value);
    }

    void setVec3(const char* name, const glm::vec3& value) const
    {
        glUniform3fv(glGetUniformLocation(id, name), 1, glm::value_ptr(value));
    }

    // Phase 5: the normal matrix is a mat3, not a mat4. Sending it as a mat4 would
    // let the model matrix's translation column leak into a direction.
    void setMat3(const char* name, const glm::mat3& value) const
    {
        glUniformMatrix3fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(value));
    }

    void setMat4(const char* name, const glm::mat4& value) const
    {
        glUniformMatrix4fv(glGetUniformLocation(id, name), 1, GL_FALSE, glm::value_ptr(value));
    }

private:
    static bool readFile(const std::string& path, std::string& out)
    {
        std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
        if (!file) {
            std::fprintf(stderr,
                "\n[shader] cannot open '%s'\n"
                "         The shaders are copied next to the executable by the CMake POST_BUILD\n"
                "         step, so this almost always means the working directory is wrong.\n"
                "         Check the \"cwd\" line in .vscode/launch.json (guide 0.3.1).\n\n",
                path.c_str());
            return false;
        }

        std::ostringstream ss;
        ss << file.rdbuf();
        out = ss.str();

        if (out.empty()) {
            std::fprintf(stderr, "\n[shader] '%s' is empty\n\n", path.c_str());
            return false;
        }
        return true;
    }

    // Inserts `common` immediately after the #version directive, which the GLSL
    // spec requires to be the first thing in a shader — prepending ahead of it is
    // a compile error, so the block cannot simply be concatenated on the front.
    //
    // A #line directive is emitted afterwards so the driver keeps reporting errors
    // against the real line numbers in the .vert / .frag file. Without it every
    // compile error would be off by the length of the shared block, which is
    // exactly the kind of silent papercut Phase 2 set out to avoid.
    static std::string spliceAfterVersion(const std::string& source,
                                          const std::string& common,
                                          const std::string& label)
    {
        const size_t versionAt = source.find("#version");
        if (versionAt == std::string::npos) {
            std::fprintf(stderr,
                "[shader] '%s' has no #version directive; prepending the shared block anyway\n",
                label.c_str());
            return common + source;
        }

        size_t lineEnd = source.find('\n', versionAt);
        if (lineEnd == std::string::npos) lineEnd = source.size();
        else                              ++lineEnd;          // keep the newline

        // The #version line is line 1, so the file body resumes at line 2.
        return source.substr(0, lineEnd) + common + "\n#line 2\n" + source.substr(lineEnd);
    }

    static GLuint compileStage(GLenum type, const std::string& source, const std::string& label)
    {
        const GLuint shader = glCreateShader(type);
        const char*  src    = source.c_str();
        glShaderSource(shader, 1, &src, NULL);
        glCompileShader(shader);

        GLint compiled = GL_FALSE;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

        // Print the log even on success — drivers emit useful warnings there too.
        printInfoLog(shader, false, label, compiled == GL_TRUE);

        if (compiled != GL_TRUE) {
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    static bool checkLink(GLuint program, const std::string& vertPath, const std::string& fragPath)
    {
        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);

        const std::string label = vertPath + " + " + fragPath;
        printInfoLog(program, true, label, linked == GL_TRUE);

        return linked == GL_TRUE;
    }

    // One log printer for both shader objects and program objects — the query pair
    // differs, the reporting should not.
    static void printInfoLog(GLuint object, bool isProgram, const std::string& label, bool succeeded)
    {
        GLint length = 0;
        if (isProgram) glGetProgramiv(object, GL_INFO_LOG_LENGTH, &length);
        else           glGetShaderiv(object, GL_INFO_LOG_LENGTH, &length);

        std::string log;
        if (length > 1) {
            std::vector<char> buffer((size_t)length);
            GLsizei written = 0;
            if (isProgram) glGetProgramInfoLog(object, length, &written, buffer.data());
            else           glGetShaderInfoLog(object, length, &written, buffer.data());
            log.assign(buffer.data(), (size_t)written);
        }

        if (succeeded && log.empty())
            return;   // clean compile or link: stay quiet

        const char* stage  = isProgram ? "link" : "compile";
        std::FILE*  stream = succeeded ? stdout : stderr;

        std::fprintf(stream,
            "\n=====================================================================\n"
            " GLSL %s %s: %s\n"
            "---------------------------------------------------------------------\n"
            "%s\n"
            "=====================================================================\n\n",
            stage, succeeded ? "warning" : "FAILED", label.c_str(),
            log.empty() ? "(driver returned no log)" : log.c_str());
        std::fflush(stream);
    }
};
