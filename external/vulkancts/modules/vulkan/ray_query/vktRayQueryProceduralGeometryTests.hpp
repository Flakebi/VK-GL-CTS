#ifndef _VKTRAYQUERYPROCEDURALGEOMETRYTESTS_HPP
#define _VKTRAYQUERYPROCEDURALGEOMETRYTESTS_HPP
/*-------------------------------------------------------------------------
 * Vulkan Conformance Tests
 * ------------------------
 *
 * Copyright (c) 2020 The Khronos Group Inc.
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
 *//*!
 * \file
 * \brief Test procedural geometry with complex bouding box sets
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "tcuTestCase.hpp"

namespace vkt
{
namespace RayQuery
{

tcu::TestCaseGroup *createProceduralGeometryTests(tcu::TestContext &testCtx);

} // namespace RayQuery
} // namespace vkt

#endif // _VKTRAYQUERYPROCEDURALGEOMETRYTESTS_HPP
