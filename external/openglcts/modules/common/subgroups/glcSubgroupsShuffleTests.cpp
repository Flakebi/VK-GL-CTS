/*------------------------------------------------------------------------
 * OpenGL Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2017-2019 The Khronos Group Inc.
 * Copyright (c) 2017 Codeplay Software Ltd.
 * Copyright (c) 2019 NVIDIA Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */ /*!
 * \file
 * \brief Subgroups Tests
 */ /*--------------------------------------------------------------------*/

#include "glcSubgroupsShuffleTests.hpp"
#include "glcSubgroupsTestsUtils.hpp"

#include <string>
#include <vector>

using namespace tcu;
using namespace std;

namespace glc
{
namespace subgroups
{
namespace
{
enum OpType
{
    OPTYPE_SHUFFLE = 0,
    OPTYPE_SHUFFLE_XOR,
    OPTYPE_SHUFFLE_UP,
    OPTYPE_SHUFFLE_DOWN,
    OPTYPE_LAST
};

static bool checkVertexPipelineStages(std::vector<const void *> datas, uint32_t width, uint32_t)
{
    return glc::subgroups::check(datas, width, 1);
}

static bool checkComputeStage(std::vector<const void *> datas, const uint32_t numWorkgroups[3],
                              const uint32_t localSize[3], uint32_t)
{
    return glc::subgroups::checkCompute(datas, numWorkgroups, localSize, 1);
}

std::string getOpTypeName(int opType)
{
    switch (opType)
    {
    default:
        DE_FATAL("Unsupported op type");
        return "";
    case OPTYPE_SHUFFLE:
        return "subgroupShuffle";
    case OPTYPE_SHUFFLE_XOR:
        return "subgroupShuffleXor";
    case OPTYPE_SHUFFLE_UP:
        return "subgroupShuffleUp";
    case OPTYPE_SHUFFLE_DOWN:
        return "subgroupShuffleDown";
    }
}

struct CaseDefinition
{
    int opType;
    ShaderStageFlags shaderStage;
    Format format;
};

const std::string to_string(int x)
{
    std::ostringstream oss;
    oss << x;
    return oss.str();
}

const std::string DeclSource(CaseDefinition caseDef, int baseBinding)
{
    return "layout(binding = " + to_string(baseBinding) +
           ", std430) readonly buffer BufferB0\n"
           "{\n"
           "  " +
           subgroups::getFormatNameForGLSL(caseDef.format) +
           " data1[];\n"
           "};\n"
           "layout(binding = " +
           to_string(baseBinding + 1) +
           ", std430) readonly buffer BufferB1\n"
           "{\n"
           "  uint data2[];\n"
           "};\n";
}

const std::string TestSource(CaseDefinition caseDef)
{
    std::string idTable[OPTYPE_LAST];
    idTable[OPTYPE_SHUFFLE]      = "id_in";
    idTable[OPTYPE_SHUFFLE_XOR]  = "gl_SubgroupInvocationID ^ id_in";
    idTable[OPTYPE_SHUFFLE_UP]   = "gl_SubgroupInvocationID - id_in";
    idTable[OPTYPE_SHUFFLE_DOWN] = "gl_SubgroupInvocationID + id_in";

    const std::string testSource =
        "  uint temp_res;\n"
        "  uvec4 mask = subgroupBallot(true);\n"
        "  uint id_in = data2[gl_SubgroupInvocationID] & (gl_SubgroupSize - 1u);\n"
        "  " +
        subgroups::getFormatNameForGLSL(caseDef.format) + " op = " + getOpTypeName(caseDef.opType) +
        "(data1[gl_SubgroupInvocationID], id_in);\n"
        "  uint id = " +
        idTable[caseDef.opType] +
        ";\n"
        "  if ((id < gl_SubgroupSize) && subgroupBallotBitExtract(mask, id))\n"
        "  {\n"
        "    temp_res = (op == data1[id]) ? 1u : 0u;\n"
        "  }\n"
        "  else\n"
        "  {\n"
        "    temp_res = 1u; // Invocation we read from was inactive, so we can't verify results!\n"
        "  }\n";

    return testSource;
}

void initFrameBufferPrograms(SourceCollections &programCollection, CaseDefinition caseDef)
{
    subgroups::setFragmentShaderFrameBuffer(programCollection);

    if (SHADER_STAGE_VERTEX_BIT != caseDef.shaderStage)
        subgroups::setVertexShaderFrameBuffer(programCollection);

    const std::string extSource = (OPTYPE_SHUFFLE == caseDef.opType || OPTYPE_SHUFFLE_XOR == caseDef.opType) ?
                                      "#extension GL_KHR_shader_subgroup_shuffle: enable\n" :
                                      "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";

    const std::string testSource = TestSource(caseDef);

    if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
    {
        std::ostringstream vertexSrc;
        vertexSrc << "${VERSION_DECL}\n"
                  << extSource << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
                  << "layout(location = 0) in highp vec4 in_position;\n"
                  << "layout(location = 0) out float result;\n"
                  << "layout(binding = 0, std140) uniform Buffer0\n"
                  << "{\n"
                  << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1["
                  << subgroups::maxSupportedSubgroupSize() << "];\n"
                  << "};\n"
                  << "layout(binding = 1, std140) uniform Buffer1\n"
                  << "{\n"
                  << "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
                  << "};\n"
                  << "\n"
                  << "void main (void)\n"
                  << "{\n"
                  << testSource << "  result = float(temp_res);\n"
                  << "  gl_Position = in_position;\n"
                  << "  gl_PointSize = 1.0f;\n"
                  << "}\n";
        programCollection.add("vert") << glu::VertexSource(vertexSrc.str());
    }
    else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
    {
        std::ostringstream geometry;

        geometry << "${VERSION_DECL}\n"
                 << extSource << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
                 << "layout(points) in;\n"
                 << "layout(points, max_vertices = 1) out;\n"
                 << "layout(location = 0) out float out_color;\n"
                 << "layout(binding = 0, std140) uniform Buffer0\n"
                 << "{\n"
                 << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1["
                 << subgroups::maxSupportedSubgroupSize() << "];\n"
                 << "};\n"
                 << "layout(binding = 1, std140) uniform Buffer1\n"
                 << "{\n"
                 << "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
                 << "};\n"
                 << "\n"
                 << "void main (void)\n"
                 << "{\n"
                 << testSource << "  out_color = float(temp_res);\n"
                 << "  gl_Position = gl_in[0].gl_Position;\n"
                 << "  EmitVertex();\n"
                 << "  EndPrimitive();\n"
                 << "}\n";

        programCollection.add("geometry") << glu::GeometrySource(geometry.str());
    }
    else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
    {
        std::ostringstream controlSource;

        controlSource << "${VERSION_DECL}\n"
                      << extSource << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
                      << "layout(vertices = 2) out;\n"
                      << "layout(location = 0) out float out_color[];\n"
                      << "layout(binding = 0, std140) uniform Buffer0\n"
                      << "{\n"
                      << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1["
                      << subgroups::maxSupportedSubgroupSize() << "];\n"
                      << "};\n"
                      << "layout(binding = 1, std140) uniform Buffer1\n"
                      << "{\n"
                      << "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
                      << "};\n"
                      << "\n"
                      << "void main (void)\n"
                      << "{\n"
                      << "  if (gl_InvocationID == 0)\n"
                      << "  {\n"
                      << "    gl_TessLevelOuter[0] = 1.0f;\n"
                      << "    gl_TessLevelOuter[1] = 1.0f;\n"
                      << "  }\n"
                      << testSource << "  out_color[gl_InvocationID] = float(temp_res);\n"
                      << "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                      << "}\n";

        programCollection.add("tesc") << glu::TessellationControlSource(controlSource.str());
        subgroups::setTesEvalShaderFrameBuffer(programCollection);
    }
    else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
    {
        std::ostringstream evaluationSource;
        evaluationSource << "${VERSION_DECL}\n"
                         << extSource << "#extension GL_KHR_shader_subgroup_ballot: enable\n"
                         << "layout(isolines, equal_spacing, ccw ) in;\n"
                         << "layout(location = 0) out float out_color;\n"
                         << "layout(binding = 0, std140) uniform Buffer0\n"
                         << "{\n"
                         << "  " << subgroups::getFormatNameForGLSL(caseDef.format) << " data1["
                         << subgroups::maxSupportedSubgroupSize() << "];\n"
                         << "};\n"
                         << "layout(binding = 1, std140) uniform Buffer1\n"
                         << "{\n"
                         << "  uint data2[" << subgroups::maxSupportedSubgroupSize() << "];\n"
                         << "};\n"
                         << "\n"
                         << "void main (void)\n"
                         << "{\n"
                         << testSource << "  out_color = float(temp_res);\n"
                         << "  gl_Position = mix(gl_in[0].gl_Position, gl_in[1].gl_Position, gl_TessCoord.x);\n"
                         << "}\n";

        subgroups::setTesCtrlShaderFrameBuffer(programCollection);
        programCollection.add("tese") << glu::TessellationEvaluationSource(evaluationSource.str());
    }
    else
    {
        DE_FATAL("Unsupported shader stage");
    }
}

void initPrograms(SourceCollections &programCollection, CaseDefinition caseDef)
{
    const std::string versionSource = "${VERSION_DECL}\n";
    const std::string vSource       = "#extension GL_KHR_shader_subgroup_ballot: enable\n";
    const std::string eSource       = (OPTYPE_SHUFFLE == caseDef.opType || OPTYPE_SHUFFLE_XOR == caseDef.opType) ?
                                          "#extension GL_KHR_shader_subgroup_shuffle: enable\n" :
                                          "#extension GL_KHR_shader_subgroup_shuffle_relative: enable\n";
    const std::string extSource     = vSource + eSource;

    const std::string testSource = TestSource(caseDef);

    if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
    {
        std::ostringstream src;

        src << versionSource + extSource << "layout (${LOCAL_SIZE_X}, ${LOCAL_SIZE_Y}, ${LOCAL_SIZE_Z}) in;\n"
            << "layout(binding = 0, std430) buffer Buffer0\n"
            << "{\n"
            << "  uint result[];\n"
            << "};\n"
            << DeclSource(caseDef, 1) << "\n"
            << "void main (void)\n"
            << "{\n"
            << "  uvec3 globalSize = gl_NumWorkGroups * gl_WorkGroupSize;\n"
            << "  highp uint offset = globalSize.x * ((globalSize.y * "
               "gl_GlobalInvocationID.z) + gl_GlobalInvocationID.y) + "
               "gl_GlobalInvocationID.x;\n"
            << testSource << "  result[offset] = temp_res;\n"
            << "}\n";

        programCollection.add("comp") << glu::ComputeSource(src.str());
    }
    else
    {
        const std::string declSource = DeclSource(caseDef, 4);

        {
            const string vertex =
                versionSource + extSource +
                "layout(binding = 0, std430) buffer Buffer0\n"
                "{\n"
                "  uint result[];\n"
                "} b0;\n" +
                declSource +
                "\n"
                "void main (void)\n"
                "{\n" +
                testSource +
                "  b0.result[gl_VertexID] = temp_res;\n"
                "  float pixelSize = 2.0f/1024.0f;\n"
                "  float pixelPosition = pixelSize/2.0f - 1.0f;\n"
                "  gl_Position = vec4(float(gl_VertexID) * pixelSize + pixelPosition, 0.0f, 0.0f, 1.0f);\n"
                "  gl_PointSize = 1.0f;\n"
                "}\n";

            programCollection.add("vert") << glu::VertexSource(vertex);
        }

        {
            const string tesc = versionSource + extSource +
                                "layout(vertices=1) out;\n"
                                "layout(binding = 1, std430)  buffer Buffer1\n"
                                "{\n"
                                "  uint result[];\n"
                                "} b1;\n" +
                                declSource +
                                "\n"
                                "void main (void)\n"
                                "{\n" +
                                testSource +
                                "  b1.result[gl_PrimitiveID] = temp_res;\n"
                                "  if (gl_InvocationID == 0)\n"
                                "  {\n"
                                "    gl_TessLevelOuter[0] = 1.0f;\n"
                                "    gl_TessLevelOuter[1] = 1.0f;\n"
                                "  }\n"
                                "  gl_out[gl_InvocationID].gl_Position = gl_in[gl_InvocationID].gl_Position;\n"
                                "}\n";

            programCollection.add("tesc") << glu::TessellationControlSource(tesc);
        }

        {
            const string tese = versionSource + extSource +
                                "layout(isolines) in;\n"
                                "layout(binding = 2, std430) buffer Buffer2\n"
                                "{\n"
                                "  uint result[];\n"
                                "} b2;\n" +
                                declSource +
                                "\n"
                                "void main (void)\n"
                                "{\n" +
                                testSource +
                                "  b2.result[gl_PrimitiveID * 2 + int(gl_TessCoord.x + 0.5)] = temp_res;\n"
                                "  float pixelSize = 2.0f/1024.0f;\n"
                                "  gl_Position = gl_in[0].gl_Position + gl_TessCoord.x * pixelSize / 2.0f;\n"
                                "}\n";

            programCollection.add("tese") << glu::TessellationEvaluationSource(tese);
        }

        {
            const string geometry =
                // version is added by addGeometryShadersFromTemplate
                extSource +
                "layout(${TOPOLOGY}) in;\n"
                "layout(points, max_vertices = 1) out;\n"
                "layout(binding = 3, std430) buffer Buffer3\n"
                "{\n"
                "  uint result[];\n"
                "} b3;\n" +
                declSource +
                "\n"
                "void main (void)\n"
                "{\n" +
                testSource +
                "  b3.result[gl_PrimitiveIDIn] = temp_res;\n"
                "  gl_Position = gl_in[0].gl_Position;\n"
                "  EmitVertex();\n"
                "  EndPrimitive();\n"
                "}\n";

            subgroups::addGeometryShadersFromTemplate(geometry, programCollection);
        }
        {
            const string fragment = versionSource + extSource +
                                    "precision highp int;\n"
                                    "precision highp float;\n"
                                    "layout(location = 0) out uint result;\n" +
                                    declSource +
                                    "void main (void)\n"
                                    "{\n" +
                                    testSource +
                                    "  result = temp_res;\n"
                                    "}\n";

            programCollection.add("fragment") << glu::FragmentSource(fragment);
        }

        subgroups::addNoSubgroupShader(programCollection);
    }
}

void supportedCheck(Context &context, CaseDefinition caseDef)
{
    if (!subgroups::isSubgroupSupported(context))
        TCU_THROW(NotSupportedError, "Subgroup operations are not supported");

    switch (caseDef.opType)
    {
    case OPTYPE_SHUFFLE:
    case OPTYPE_SHUFFLE_XOR:
        if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_SHUFFLE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
        }
        break;
    default:
        if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
        }
        break;
    }

    if (subgroups::isDoubleFormat(caseDef.format) && !subgroups::isDoubleSupportedForDevice(context))
        TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
}

tcu::TestStatus noSSBOtest(Context &context, const CaseDefinition caseDef)
{
    if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
    {
        if (subgroups::areSubgroupOperationsRequiredForStage(caseDef.shaderStage))
        {
            return tcu::TestStatus::fail("Shader stage " + subgroups::getShaderStageName(caseDef.shaderStage) +
                                         " is required to support subgroup operations!");
        }
        else
        {
            TCU_THROW(NotSupportedError, "Device does not support subgroup operations for this stage");
        }
    }

    subgroups::SSBOData inputData[2];
    inputData[0].format         = caseDef.format;
    inputData[0].layout         = subgroups::SSBOData::LayoutStd140;
    inputData[0].numElements    = subgroups::maxSupportedSubgroupSize();
    inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
    inputData[0].binding        = 0u;

    inputData[1].format         = FORMAT_R32_UINT;
    inputData[1].layout         = subgroups::SSBOData::LayoutStd140;
    inputData[1].numElements    = inputData[0].numElements;
    inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;
    inputData[1].binding        = 1u;

    if (SHADER_STAGE_VERTEX_BIT == caseDef.shaderStage)
        return subgroups::makeVertexFrameBufferTest(context, FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages);
    else if (SHADER_STAGE_GEOMETRY_BIT == caseDef.shaderStage)
        return subgroups::makeGeometryFrameBufferTest(context, FORMAT_R32_UINT, inputData, 2,
                                                      checkVertexPipelineStages);
    else if (SHADER_STAGE_TESS_CONTROL_BIT == caseDef.shaderStage)
        return subgroups::makeTessellationEvaluationFrameBufferTest(
            context, FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, SHADER_STAGE_TESS_CONTROL_BIT);
    else if (SHADER_STAGE_TESS_EVALUATION_BIT == caseDef.shaderStage)
        return subgroups::makeTessellationEvaluationFrameBufferTest(
            context, FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, SHADER_STAGE_TESS_EVALUATION_BIT);
    else
        TCU_THROW(InternalError, "Unhandled shader stage");
}

tcu::TestStatus test(Context &context, const CaseDefinition caseDef)
{
    switch (caseDef.opType)
    {
    case OPTYPE_SHUFFLE:
    case OPTYPE_SHUFFLE_XOR:
        if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_SHUFFLE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle operations");
        }
        break;
    default:
        if (!subgroups::isSubgroupFeatureSupportedForDevice(context, SUBGROUP_FEATURE_SHUFFLE_RELATIVE_BIT))
        {
            TCU_THROW(NotSupportedError, "Device does not support subgroup shuffle relative operations");
        }
        break;
    }

    if (subgroups::isDoubleFormat(caseDef.format) && !subgroups::isDoubleSupportedForDevice(context))
    {
        TCU_THROW(NotSupportedError, "Device does not support subgroup double operations");
    }

    if (SHADER_STAGE_COMPUTE_BIT == caseDef.shaderStage)
    {
        if (!subgroups::areSubgroupOperationsSupportedForStage(context, caseDef.shaderStage))
        {
            return tcu::TestStatus::fail("Shader stage " + subgroups::getShaderStageName(caseDef.shaderStage) +
                                         " is required to support subgroup operations!");
        }
        subgroups::SSBOData inputData[2];
        inputData[0].format         = caseDef.format;
        inputData[0].layout         = subgroups::SSBOData::LayoutStd430;
        inputData[0].numElements    = subgroups::maxSupportedSubgroupSize();
        inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
        inputData[0].binding        = 1u;

        inputData[1].format         = FORMAT_R32_UINT;
        inputData[1].layout         = subgroups::SSBOData::LayoutStd430;
        inputData[1].numElements    = inputData[0].numElements;
        inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;
        inputData[1].binding        = 2u;

        return subgroups::makeComputeTest(context, FORMAT_R32_UINT, inputData, 2, checkComputeStage);
    }

    else
    {
        int supportedStages = context.getDeqpContext().getContextInfo().getInt(GL_SUBGROUP_SUPPORTED_STAGES_KHR);

        ShaderStageFlags stages = (ShaderStageFlags)(caseDef.shaderStage & supportedStages);

        if (SHADER_STAGE_FRAGMENT_BIT != stages && !subgroups::isVertexSSBOSupportedForDevice(context))
        {
            if ((stages & SHADER_STAGE_FRAGMENT_BIT) == 0)
                TCU_THROW(NotSupportedError, "Device does not support vertex stage SSBO writes");
            else
                stages = SHADER_STAGE_FRAGMENT_BIT;
        }

        if ((ShaderStageFlags)0u == stages)
            TCU_THROW(NotSupportedError, "Subgroup operations are not supported for any graphic shader");

        subgroups::SSBOData inputData[2];
        inputData[0].format         = caseDef.format;
        inputData[0].layout         = subgroups::SSBOData::LayoutStd430;
        inputData[0].numElements    = subgroups::maxSupportedSubgroupSize();
        inputData[0].initializeType = subgroups::SSBOData::InitializeNonZero;
        inputData[0].binding        = 4u;
        inputData[0].stages         = stages;

        inputData[1].format         = FORMAT_R32_UINT;
        inputData[1].layout         = subgroups::SSBOData::LayoutStd430;
        inputData[1].numElements    = inputData[0].numElements;
        inputData[1].initializeType = subgroups::SSBOData::InitializeNonZero;
        inputData[1].binding        = 5u;
        inputData[1].stages         = stages;

        return subgroups::allStages(context, FORMAT_R32_UINT, inputData, 2, checkVertexPipelineStages, stages);
    }
}
} // namespace

deqp::TestCaseGroup *createSubgroupsShuffleTests(deqp::Context &testCtx)
{

    de::MovePtr<deqp::TestCaseGroup> graphicGroup(
        new deqp::TestCaseGroup(testCtx, "graphics", "Subgroup shuffle category tests: graphics"));
    de::MovePtr<deqp::TestCaseGroup> computeGroup(
        new deqp::TestCaseGroup(testCtx, "compute", "Subgroup shuffle category tests: compute"));
    de::MovePtr<deqp::TestCaseGroup> framebufferGroup(
        new deqp::TestCaseGroup(testCtx, "framebuffer", "Subgroup shuffle category tests: framebuffer"));

    const Format formats[] = {
        FORMAT_R32_SINT,   FORMAT_R32G32_SINT,   FORMAT_R32G32B32_SINT,   FORMAT_R32G32B32A32_SINT,
        FORMAT_R32_UINT,   FORMAT_R32G32_UINT,   FORMAT_R32G32B32_UINT,   FORMAT_R32G32B32A32_UINT,
        FORMAT_R32_SFLOAT, FORMAT_R32G32_SFLOAT, FORMAT_R32G32B32_SFLOAT, FORMAT_R32G32B32A32_SFLOAT,
        FORMAT_R64_SFLOAT, FORMAT_R64G64_SFLOAT, FORMAT_R64G64B64_SFLOAT, FORMAT_R64G64B64A64_SFLOAT,
        FORMAT_R32_BOOL,   FORMAT_R32G32_BOOL,   FORMAT_R32G32B32_BOOL,   FORMAT_R32G32B32A32_BOOL,
    };

    const ShaderStageFlags stages[] = {
        SHADER_STAGE_VERTEX_BIT,
        SHADER_STAGE_TESS_EVALUATION_BIT,
        SHADER_STAGE_TESS_CONTROL_BIT,
        SHADER_STAGE_GEOMETRY_BIT,
    };

    for (int formatIndex = 0; formatIndex < DE_LENGTH_OF_ARRAY(formats); ++formatIndex)
    {
        const Format format = formats[formatIndex];

        for (int opTypeIndex = 0; opTypeIndex < OPTYPE_LAST; ++opTypeIndex)
        {

            const string name = de::toLower(getOpTypeName(opTypeIndex)) + "_" + subgroups::getFormatNameForGLSL(format);

            {
                const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_ALL_GRAPHICS, format};
                SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(
                    graphicGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
            }

            {
                const CaseDefinition caseDef = {opTypeIndex, SHADER_STAGE_COMPUTE_BIT, format};
                SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(
                    computeGroup.get(), name, "", supportedCheck, initPrograms, test, caseDef);
            }

            for (int stageIndex = 0; stageIndex < DE_LENGTH_OF_ARRAY(stages); ++stageIndex)
            {
                const CaseDefinition caseDef = {opTypeIndex, stages[stageIndex], format};
                SubgroupFactory<CaseDefinition>::addFunctionCaseWithPrograms(
                    framebufferGroup.get(), name + "_" + getShaderStageName(caseDef.shaderStage), "", supportedCheck,
                    initFrameBufferPrograms, noSSBOtest, caseDef);
            }
        }
    }

    de::MovePtr<deqp::TestCaseGroup> group(
        new deqp::TestCaseGroup(testCtx, "shuffle", "Subgroup shuffle category tests"));

    group->addChild(graphicGroup.release());
    group->addChild(computeGroup.release());
    group->addChild(framebufferGroup.release());

    return group.release();
}

} // namespace subgroups
} // namespace glc
