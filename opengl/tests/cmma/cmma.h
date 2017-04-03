#ifndef GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_
#define GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sched.h>
#include <sys/resource.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl32.h>
#include <utils/Timers.h>

#include <string>
#include <sstream>
#include <iostream>


#include <WindowSurface.h>
#include <EGLUtils.h>
#include <vector>

enum CopyTextureMethod {
  // Use CopyTex{Sub}Image2D to copy from the source to the destination.
  DIRECT_COPY,
  // Draw from the source to the destination texture.
  DIRECT_DRAW,
  // Draw to an intermediate texture, and then copy to the destination texture.
  DRAW_AND_COPY,
  // CopyTexture isn't available.
  NOT_COPYABLE
};


// Apply CMAA(Conservative Morphological Anti-Aliasing) algorithm to the
// color attachments of currently bound draw framebuffer.
// Reference GL_INTEL_framebuffer_CMAA for details.
class ApplyFramebufferAttachmentCMAAINTELResourceManager {

public:
	ApplyFramebufferAttachmentCMAAINTELResourceManager();
	 ~ApplyFramebufferAttachmentCMAAINTELResourceManager();

	void Initialize();

	void Destroy();

	// Applies the algorithm to the color attachments of the currently bound draw
	// framebuffer.
	void ApplyFramebufferAttachmentCMAAINTEL(GLuint *source_cmma_bound_fbo, GLuint *rgba8_texture_main, int texture_width, int texture_height,
		GLenum internal_format_bound_fbo);

private:
 //GL's renderer version
 GLubyte* GetGLVersionInfo();
 
  // Applies the CMAA algorithm to a texture.
  void ApplyCMAAEffectTexture(GLuint source_texture,
                              GLuint dest_texture,
                              bool do_copy);

  void OnSize(GLint width, GLint height, GLuint *rgba8_texture_main);
  void ReleaseTextures();

  GLuint CreateProgram(const char* defines,
                       const char* vs_source,
                       const char* fs_source);
  GLuint CreateShader(GLenum type, const char* defines, const char* source);

  bool initialized_;
  bool textures_initialized_;
  bool is_in_gamma_correct_mode_;
  bool supports_usampler_;
  bool supports_r8_image_;
  bool is_gles31_compatible_;

  int frame_id_;

  GLint width_;
  GLint height_;

  GLuint edges0_shader_;
  GLuint edges1_shader_;
  GLuint edges_combine_shader_;
  GLuint process_and_apply_shader_;
  GLuint debug_display_edges_shader_;

  GLuint cmaa_framebuffer_;

  GLuint rgba8_texture_;
  GLuint working_color_texture_;
  GLuint edges0_texture_;
  GLuint edges1_texture_;
  GLuint mini4_edge_texture_;
  GLuint mini4_edge_depth_texture_;

  GLuint edges0_shader_result_rgba_texture_slot1_;
  GLuint edges0_shader_target_texture_slot2_;
  GLuint edges1_shader_result_edge_texture_;
  GLuint process_and_apply_shader_result_rgba_texture_slot1_;
  GLuint edges_combine_shader_result_edge_texture_;

  static const char vert_str_[];
  static const char cmaa_frag_s1_[];
  static const char cmaa_frag_s2_[];	
	
};































#endif // GPU_COMMAND_BUFFER_SERVICE_GLES2_CMD_APPLY_FRAMEBUFFER_ATTACHMENT_CMAA_INTEL_H_


