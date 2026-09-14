// SPDX-License-Identifier: MIT
// AliceEngine-Singularity — Foundation/FileSystem.h
//
// std::filesystem 을 얇게 감싼다. 감싸는 이유는 두 가지다.
//   1) 실패가 예외가 아니라 Diagnostic 으로 나와야 한다(엔진 전역 규칙).
//   2) Windows 에서 경로가 CP949 로 해석되는 사고를 막고 **UTF-8 을 강제**한다.
//      한글 경로에서 엔진이 죽는 사고는 이 프로젝트 환경에서 실제로 자주 났다.
#pragma once

#include "Result.h"

#include <string>
#include <vector>

namespace alice::fs {

bool Exists(const std::string& path);
bool IsDirectory(const std::string& path);
bool IsFile(const std::string& path);

Result<std::string> ReadTextFile(const std::string& path);
Result<std::vector<u8>> ReadBinaryFile(const std::string& path);

Status WriteTextFile(const std::string& path, std::string_view contents);
Status WriteBinaryFile(const std::string& path, const std::vector<u8>& bytes);

Status CreateDirectories(const std::string& path);

/// 확장자 필터(예: ".yaml"). 비우면 전부. recursive 면 하위까지.
/// 반환 경로는 항상 '/' 구분자의 UTF-8 문자열이다.
std::vector<std::string> ListFiles(const std::string& directory,
                                   std::string_view extension = {},
                                   bool recursive = true);

std::string Extension(const std::string& path);   ///< ".yaml" (점 포함, 소문자)
std::string FileName(const std::string& path);    ///< "player.actor.yaml"
std::string FileStem(const std::string& path);    ///< "player.actor"
std::string ParentPath(const std::string& path);
std::string Join(std::string_view a, std::string_view b);

/// 항상 '/' 를 쓰도록 정규화한다. 문서 안 경로 비교를 안정시킨다.
std::string Normalize(std::string_view path);

/// 실행 파일이 있는 디렉터리.
std::string ExecutableDirectory();

/// 현재 작업 디렉터리.
std::string CurrentDirectory();

} // namespace alice::fs
