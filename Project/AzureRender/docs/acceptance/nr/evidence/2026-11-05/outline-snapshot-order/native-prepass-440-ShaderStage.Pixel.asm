Shader hash 24725d64-55934152-f293f3d5-42f58868

ps_5_0
      dcl_globalFlags refactoringAllowed
      dcl_constantbuffer cb0[27], immediateIndexed
      dcl_constantbuffer cb1[13], immediateIndexed
      dcl_sampler s0, mode_default
      dcl_resource_texture2d (float,float,float,float) t0
      dcl_input_ps linear v1.xy
      dcl_output o0.xyzw
      dcl_temps 1
   0: sample_b(texture2d)(float,float,float,float) r0.x, v1.xyxx, t0.xyzw, s0, cb0[26].x
   1: add r0.x, r0.x, -cb1[12].x
   2: lt r0.x, r0.x, l(0)
   3: discard_nz r0.x
   4: mov o0.xyzw, l(0, 0, 0, 0)
   5: ret
