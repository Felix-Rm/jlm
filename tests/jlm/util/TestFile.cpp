/*
 * Copyright 2018 Nico Reißmann <nico.reissmann@gmail.com>
 * Copyright 2025 Håvard Krogstie <krogstie.havard@gmail.com>
 * See COPYING for terms of redistribution.
 */

#include <test-registry.hpp>

#include <jlm/util/file.hpp>

#include <cassert>
#include <vector>

static int
TestFilePathMethods()
{
  const jlm::util::filepath f("/tmp/archive.tar.gz");

  assert(f.to_str() == "/tmp/archive.tar.gz");
  assert(f.name() == "archive.tar.gz");
  assert(f.base() == "archive");
  assert(f.suffix() == "gz");
  assert(f.complete_suffix() == "tar.gz");
  assert(f.Dirname() == "/tmp/");

  std::vector<std::pair<std::string, std::string>> pathPairs = { { "/tmp/jlm/", "/tmp/" },
                                                                 { "/tmp/", "/" },
                                                                 { "d/d2/file.txt", "d/d2/" },
                                                                 { "test.txt", "" },
                                                                 { "./test2.txt", "./" },
                                                                 { "/", "/" },
                                                                 { ".", "" },
                                                                 { "", "" } };
  for (const auto & [fullPath, path] : pathPairs)
  {
    assert(jlm::util::filepath(fullPath).Dirname() == path);
  }

  return 0;
}

JLM_UNIT_TEST_REGISTER("jlm/util/TestFile-TestFilePathMethods", TestFilePathMethods)

static int
TestCreateDirectory()
{
  const auto filePath = jlm::util::filepath::TempDirectoryPath().Join("jlm-test-create-dir");

  // Remove the directory if it survived from a previous test
  if (filePath.Exists())
    std::filesystem::remove(filePath.to_str());
  assert(!filePath.Exists());

  // Act
  filePath.CreateDirectory();

  // Assert that the directory now exists
  assert(filePath.Exists() && filePath.IsDirectory());

  // Try creating a directory that already exists, should be no issue
  filePath.CreateDirectory();

  // Try creating a directory in a location that does not exist
  try
  {
    jlm::util::filepath noSuchParent("/non-existant/test-dir");
    noSuchParent.CreateDirectory();
    assert(false);
  }
  catch (...)
  {}

  // Cleanup
  std::filesystem::remove(filePath.to_str());

  return 0;
}

JLM_UNIT_TEST_REGISTER("jlm/util/TestFile-TestCreateDirectory", TestCreateDirectory)

static int
TestFilepathJoin()
{
  const jlm::util::filepath path1("tmp");
  const jlm::util::filepath path2("a/b/");
  const jlm::util::filepath path3("/c/d");

  assert(path1.Join(path2).to_str() == "tmp/a/b/");
  assert(path2.Join(path1).to_str() == "a/b/tmp");

  assert(path1.Join(path3).to_str() == "/c/d");
  assert(path3.Join(path1).to_str() == "/c/d/tmp");

  return 0;
}

JLM_UNIT_TEST_REGISTER("jlm/util/TestFile-TestFilepathJoin", TestFilepathJoin)

static int
TestCreateUniqueFileName()
{
  // Arrange
  auto randomString = jlm::util::CreateRandomAlphanumericString(6);
  auto tmpDirectory = jlm::util::filepath::TempDirectoryPath().Join(randomString);

  // Act
  auto filePath = jlm::util::filepath::CreateUniqueFileName(tmpDirectory, "myPrefix", "mySuffix");

  // Assert
  assert(filePath.Dirname() == (tmpDirectory.to_str() + "/"));

  return 0;
}

JLM_UNIT_TEST_REGISTER("jlm/util/TestFile-TestCreateUniqueFileName", TestCreateUniqueFileName)
