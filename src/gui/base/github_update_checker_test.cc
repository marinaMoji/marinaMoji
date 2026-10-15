#include "gui/base/github_update_checker.h"

#include <QByteArray>
#include <QString>
#include <QTemporaryFile>

#include "testing/gunit.h"

namespace mozc::gui {
namespace {

// SHA-256 of the ASCII string "abc" (FIPS 180-2 test vector).
constexpr char kAbcSha256[] =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

class FileMatchesSha256Test : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(file_.open());
    file_.write("abc");
    file_.flush();
  }

  QTemporaryFile file_;
};

TEST_F(FileMatchesSha256Test, MatchingDigest) {
  EXPECT_TRUE(GitHubUpdateChecker::FileMatchesSha256(
      file_.fileName(), QString::fromLatin1(kAbcSha256)));
}

TEST_F(FileMatchesSha256Test, MatchingDigestIsCaseInsensitive) {
  EXPECT_TRUE(GitHubUpdateChecker::FileMatchesSha256(
      file_.fileName(), QString::fromLatin1(kAbcSha256).toUpper()));
}

TEST_F(FileMatchesSha256Test, MismatchingDigest) {
  EXPECT_FALSE(GitHubUpdateChecker::FileMatchesSha256(
      file_.fileName(), QString(64, QLatin1Char('0'))));
}

TEST_F(FileMatchesSha256Test, EmptyDigestMeansNothingToCompare) {
  // GitHub published no digest for old releases; same as the background
  // updaters, the download is then accepted.
  EXPECT_TRUE(GitHubUpdateChecker::FileMatchesSha256(file_.fileName(),
                                                     QString()));
}

TEST_F(FileMatchesSha256Test, MissingFileNeverMatchesADigest) {
  EXPECT_FALSE(GitHubUpdateChecker::FileMatchesSha256(
      file_.fileName() + QStringLiteral(".does-not-exist"),
      QString::fromLatin1(kAbcSha256)));
}

}  // namespace
}  // namespace mozc::gui
