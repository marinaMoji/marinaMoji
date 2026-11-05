// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: BSD-3-Clause

#include "composer_ids/ids_table.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "base/file_util.h"
#include "base/strings/unicode.h"
#include "base/system_util.h"
#include "composer_ids/ids_operator.h"

namespace mozc {
namespace composer_ids {
namespace {

constexpr std::string_view kIdsFile = "ids_table.tsv";
constexpr std::string_view kRadicalFormsFile = "radical_forms.tsv";
constexpr std::string_view kStrokeCountsFile = "stroke_counts.tsv";

// The class of the first stroke (see IdsTable::LeadingStroke) of the strokes,
// radical forms and common components that decompositions bottom out in.
// Written by hand for marinaMoji (BSD-3-Clause, like this file), following
// the mainland Chinese five-way convention: rising strokes count as
// horizontal, right-falling strokes as dots, every bend and hook (竖钩 in 小
// and 水 too) as turning. Characters whose first stroke differs between the
// Chinese and Japanese orders (馬, 長, 右, 皮, 乃 …) are left out on purpose;
// they sort after the five classes unless a decomposition decides.
struct LeadingStrokeGroup {
  int stroke_class;
  const char *chars;  // UTF-8, one character each
};
constexpr LeadingStrokeGroup kLeadingStrokeGroups[] = {
    {1,
     "一㇀二三十干于工土士王玉丰耒天夫开井未末正平五元"
     "云示下不丁七万丈与才寸大太犬木本戈弋甘其革酉西两"
     "而耳豆雨面百石厂匚廿世古可至互瓦牙尤巫亞来表事吏"
     "再丙頁夏東車辰走赤青束考老者耶戸臣支求豕甫夹奉春"
     "黄黃扌⺘𤣩耂⺹艹⺾车尢麦比甚巨歺戊戌𠦝"},
    {2,
     "丨口囗日曰目田由甲申电中虫冂上止卜山貝見只足皿罒"
     "⺫罓尚⺌黒里果骨四史央内吅品巾虍齒鹵业北非典曲少"
     "光当門刂⺉𧾷⻊黑⺊贝齿肉鬥占"},
    {3,
     "丿人亻⺅𠆢入八儿几九千川久夕勹匕斤爪⺤爫父片牛牜"
     "生失矢禾白自舌舟竹⺮𥫗臼血行彳金釒钅⻐食飠𩙿饣⻟"
     "魚鳥身角丘年午手毛气欠犭⺨月用風瓜采豸隹鬼香夂攵"
     "彡缶后垂重乎我免先及乏乍系鼠兵乘鼻⺈鱼𦥑𠂊谷𠂉㐅"
     "鸟每氏爻告朱杀與"},
    {4,
     "丶⺀亠冫氵⺡宀冖广疒立言訁讠⻈文方主火灬⺣米羊首"
     "关美义斗心忄⺖⺗必永市交亡六亥玄衣衤⻂礻⺭辶⻌⻍"
     "之户鹿麻高京齊门兑兰半并前益良卒帝音章竟产离亨龍"
     "⺍丷䒑𦍌龹䜌"},
    {5,
     "乙乚乛𠃌𠃊亅了子也习刀力又马弓己已巳尸尹卩阝⻖⻏"
     "凵厶幺糸糹纟⺯女母毋丑民艮水氺⺢小承予矛聿隶刃叉"
     "飞飛廴巛彐癶巴彑尺⺄"},
};

int BuiltinLeadingStroke(std::string_view ch) {
  static const absl::flat_hash_map<std::string, int> *const table = [] {
    auto *map = new absl::flat_hash_map<std::string, int>;
    for (const LeadingStrokeGroup &group : kLeadingStrokeGroups) {
      for (const std::string_view c : Utf8AsChars(group.chars)) {
        map->emplace(std::string(c), group.stroke_class);
      }
    }
    return map;
  }();
  const auto it = table->find(ch);
  return it == table->end() ? 0 : it->second;
}

std::string_view StripCarriageReturn(std::string_view line) {
  if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
  return line;
}

}  // namespace

absl::StatusOr<IdsTable> IdsTable::FromTsv(std::string_view ids_tsv,
                                           std::string_view radical_forms_tsv) {
  IdsTable table;

  for (std::string_view line : absl::StrSplit(radical_forms_tsv, '\n')) {
    line = StripCarriageReturn(line);
    if (line.empty()) continue;
    if (line.front() == '@') {
      // "@<TAB>char<TAB>position<TAB>form", e.g. "@ 手 ⿰0 扌".
      const std::vector<std::string_view> f = absl::StrSplit(line, '\t');
      if (f.size() == 4 && !f[1].empty() && !f[2].empty() && !f[3].empty()) {
        table.automatic_forms_[absl::StrCat(f[1], "\t", f[2])] = std::string(f[3]);
      }
      continue;
    }
    std::vector<std::string> family;
    for (const std::string_view form : absl::StrSplit(line, '\t')) {
      if (!form.empty()) family.emplace_back(form);
    }
    if (!family.empty()) table.families_.push_back(std::move(family));
  }

  for (const std::vector<std::string> &family : table.families_) {
    for (const std::string &form : family) {
      if (table.base_form_.contains(form)) continue;
      const std::vector<std::string> forms = table.FormsOf(form);
      table.base_form_[form] = forms.front();
    }
  }

  struct Entry {
    std::string ids;
    std::string ch;
  };
  std::vector<Entry> entries;
  int line_number = 0;
  for (std::string_view line : absl::StrSplit(ids_tsv, '\n')) {
    ++line_number;
    line = StripCarriageReturn(line);
    if (line.empty()) continue;
    const size_t tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0 || tab + 1 >= line.size() ||
        line.find('\t', tab + 1) != std::string_view::npos) {
      return absl::InvalidArgumentError(
          absl::StrCat("malformed IDS table line ", line_number));
    }
    entries.push_back({std::string(line.substr(0, tab)),
                       std::string(line.substr(tab + 1))});
  }

  table.by_ids_.reserve(entries.size());
  table.by_base_radicals_.reserve(entries.size());
  for (const Entry &entry : entries) {
    // Later entries win, as in the table the data was built for.
    table.by_ids_[entry.ids] = entry.ch;

    std::string key = table.WithBaseRadicals(entry.ids);
    auto [it, inserted] = table.by_base_radicals_.try_emplace(key, entry.ch);
    if (!inserted && it->second != entry.ch) it->second.clear();

    // A character that decomposes to itself (a stroke, a radical) is atomic.
    if (entry.ids == entry.ch) continue;
    std::vector<std::string> &list = table.by_char_[entry.ch];
    if (std::find(list.begin(), list.end(), entry.ids) == list.end()) {
      list.push_back(entry.ids);
    }
  }
  return table;
}

absl::StatusOr<IdsTable> IdsTable::LoadFromDirectory(
    std::string_view directory) {
  absl::StatusOr<std::string> ids =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kIdsFile));
  if (!ids.ok()) return ids.status();
  // The radical families only widen matching; the table works without them.
  absl::StatusOr<std::string> forms =
      FileUtil::GetContents(FileUtil::JoinPath(directory, kRadicalFormsFile));
  absl::StatusOr<IdsTable> table =
      FromTsv(*ids, forms.ok() ? std::string_view(*forms) : "");
  if (!table.ok()) return table;
  // Stroke counts only enable the stroke filter; the table works without them.
  if (absl::StatusOr<std::string> strokes = FileUtil::GetContents(
          FileUtil::JoinPath(directory, kStrokeCountsFile));
      strokes.ok()) {
    table->LoadStrokeCounts(*strokes);
  }
  return table;
}

void IdsTable::LoadStrokeCounts(std::string_view tsv) {
  stroke_counts_.clear();
  for (std::string_view line : absl::StrSplit(tsv, '\n')) {
    line = StripCarriageReturn(line);
    const size_t tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0) continue;
    std::vector<int> counts;
    for (const std::string_view value :
         absl::StrSplit(line.substr(tab + 1), ',', absl::SkipEmpty())) {
      int n = 0;
      if (absl::SimpleAtoi(value, &n) && n > 0) counts.push_back(n);
    }
    if (!counts.empty()) {
      stroke_counts_[std::string(line.substr(0, tab))] = std::move(counts);
    }
  }
}

std::string IdsTable::DefaultDirectory() {
  if (const char *env = std::getenv("MARINAMOJI_CHARACTER_COMPOSER_DIR");
      env != nullptr && *env != '\0') {
    return env;
  }
  return FileUtil::JoinPath(SystemUtil::GetServerDirectory(),
                            "character_composer");
}

namespace {

struct IdsDefaultHolder {
  std::mutex mutex;
  std::unique_ptr<IdsTable> table;
  bool attempted = false;
  bool testing_override = false;
};

IdsDefaultHolder *GetIdsDefaultHolder() {
  static IdsDefaultHolder *holder = new IdsDefaultHolder();
  return holder;
}

}  // namespace

const IdsTable *IdsTable::GetDefault() {
  IdsDefaultHolder *holder = GetIdsDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  if (holder->testing_override) {
    return holder->table.get();
  }
  if (!holder->attempted) {
    holder->attempted = true;
    if (auto loaded = LoadFromDirectory(DefaultDirectory()); loaded.ok()) {
      holder->table = std::make_unique<IdsTable>(*std::move(loaded));
    }
  }
  return holder->table.get();
}

void IdsTable::SetDefaultForTesting(std::unique_ptr<IdsTable> table) {
  IdsDefaultHolder *holder = GetIdsDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  holder->testing_override = true;
  holder->table = std::move(table);
  holder->attempted = true;
}

void IdsTable::ClearDefaultForTesting() {
  IdsDefaultHolder *holder = GetIdsDefaultHolder();
  std::lock_guard<std::mutex> lock(holder->mutex);
  holder->testing_override = false;
  holder->table.reset();
  holder->attempted = false;
}

std::vector<std::string> IdsTable::FormsOf(std::string_view ch) const {
  std::vector<std::string> forms;
  for (const std::vector<std::string> &family : families_) {
    if (std::find(family.begin(), family.end(), ch) == family.end()) continue;
    for (const std::string &form : family) {
      if (std::find(forms.begin(), forms.end(), form) == forms.end()) {
        forms.push_back(form);
      }
    }
  }
  return forms;
}

std::optional<std::string> IdsTable::NextFormOf(std::string_view ch) const {
  const std::vector<std::string> forms = FormsOf(ch);
  if (forms.size() < 2) return std::nullopt;
  const auto it = std::find(forms.begin(), forms.end(), ch);
  return forms[((it - forms.begin()) + 1) % forms.size()];
}

std::optional<std::string> IdsTable::AutomaticFormFor(
    std::string_view ch, std::string_view op, int part_index) const {
  const auto it = automatic_forms_.find(
      absl::StrCat(ch, "\t", op, part_index));
  if (it == automatic_forms_.end()) return std::nullopt;
  return it->second;
}

std::string IdsTable::WithBaseRadicals(std::string_view ids) const {
  std::string out;
  for (const std::string_view ch : Utf8AsChars(ids)) {
    const auto it = base_form_.find(ch);
    out.append(it == base_form_.end() ? ch : std::string_view(it->second));
  }
  return out;
}

std::optional<std::string> IdsTable::FindEncodedCharacter(
    std::string_view ids) const {
  if (const auto it = by_ids_.find(ids); it != by_ids_.end()) {
    return it->second;
  }
  const auto it = by_base_radicals_.find(WithBaseRadicals(ids));
  if (it == by_base_radicals_.end() || it->second.empty()) return std::nullopt;
  return it->second;
}

std::vector<std::string> IdsTable::FindDecompositions(
    std::string_view input) const {
  for (const std::string_view ch :
       Utf8AsChars(absl::StripAsciiWhitespace(input))) {
    const auto it = by_char_.find(ch);
    return it == by_char_.end() ? std::vector<std::string>() : it->second;
  }
  return {};
}

namespace {

using ComponentSet = absl::flat_hash_set<std::string>;

// A component is a single non-ASCII character that is not an operator; ASCII
// is what the data uses for placeholders like "&CDP-8C4D;".
bool IsComponent(std::string_view ch) {
  return ch.size() > 1 && !IsIdsOperator(ch);
}

}  // namespace

void IdsTable::BuildContainingIndex() const {
  if (containing_.has_value()) return;

  // character -> base forms of everything below it, over all its recorded
  // decompositions. node_hash_map keeps references stable while recursing.
  absl::node_hash_map<std::string, ComponentSet> memo;
  absl::flat_hash_set<std::string> visiting;  // guards against data cycles

  auto base_of = [this](const std::string &ch) {
    const auto it = base_form_.find(ch);
    return it == base_form_.end() ? ch : it->second;
  };

  auto expand = [&](auto &&self, const std::string &ch) -> const ComponentSet & {
    if (const auto it = memo.find(ch); it != memo.end()) return it->second;
    ComponentSet &result = memo[ch];
    const auto decompositions = by_char_.find(ch);
    if (decompositions == by_char_.end() || !visiting.insert(ch).second) {
      return result;
    }
    for (const std::string &ids : decompositions->second) {
      for (const std::string_view part : Utf8AsChars(ids)) {
        if (!IsComponent(part)) continue;
        const std::string part_str(part);
        result.insert(base_of(part_str));
        if (part_str == ch) continue;
        // Copy: |result| may not be touched while |self| fills other entries.
        const ComponentSet below = self(self, part_str);
        result.insert(below.begin(), below.end());
      }
    }
    visiting.erase(ch);
    return result;
  };

  absl::flat_hash_map<std::string, std::vector<std::string>> index;
  for (const auto &entry : by_char_) {
    const std::string &ch = entry.first;
    for (const std::string &component : expand(expand, ch)) {
      if (component != base_of(ch)) index[component].push_back(ch);
    }
  }
  for (auto &entry : index) std::sort(entry.second.begin(), entry.second.end());
  containing_ = std::move(index);
}

std::vector<std::string> IdsTable::FindContaining(
    std::string_view components) const {
  return FindContaining(components, std::nullopt);
}

std::vector<std::string> IdsTable::FindContaining(
    std::string_view components,
    const std::optional<StrokeRange> &strokes) const {
  const auto in_range = [&](const std::string &ch) {
    if (!strokes.has_value()) return true;
    const auto it = stroke_counts_.find(ch);
    return it != stroke_counts_.end() &&
           std::any_of(it->second.begin(), it->second.end(),
                       [&](int n) { return strokes->Contains(n); });
  };

  std::vector<const std::vector<std::string> *> lists;
  ComponentSet seen;
  BuildContainingIndex();
  for (const std::string_view ch :
       Utf8AsChars(absl::StripAsciiWhitespace(components))) {
    if (!IsComponent(ch)) continue;
    const auto base = base_form_.find(std::string(ch));
    const std::string key = base == base_form_.end() ? std::string(ch) : base->second;
    if (!seen.insert(key).second) continue;
    const auto it = containing_->find(key);
    if (it == containing_->end()) return {};  // nothing contains this one
    lists.push_back(&it->second);
  }
  if (lists.empty()) {
    std::vector<std::string> all;
    if (!strokes.has_value()) return all;
    for (const auto &entry : stroke_counts_) {
      if (in_range(entry.first)) all.push_back(entry.first);
    }
    std::sort(all.begin(), all.end());
    return all;
  }

  std::sort(lists.begin(), lists.end(),
            [](const auto *a, const auto *b) { return a->size() < b->size(); });
  std::vector<std::string> result;
  for (const std::string &ch : *lists.front()) {
    const bool in_all = std::all_of(
        lists.begin() + 1, lists.end(), [&](const std::vector<std::string> *list) {
          return std::binary_search(list->begin(), list->end(), ch);
        });
    if (in_all && in_range(ch)) result.push_back(ch);
  }
  return result;
}

std::optional<int> IdsTable::StrokeCount(std::string_view ch) const {
  const auto it = stroke_counts_.find(ch);
  if (it == stroke_counts_.end() || it->second.empty()) return std::nullopt;
  return it->second.front();
}

void IdsTable::LoadLeadingStrokes(std::string_view tsv) {
  leading_strokes_.clear();
  leading_cache_.clear();
  for (std::string_view line : absl::StrSplit(tsv, '\n')) {
    line = StripCarriageReturn(line);
    if (line.empty() || line.front() == '#') continue;
    const size_t tab = line.find('\t');
    if (tab == std::string_view::npos || tab == 0) continue;
    int n = 0;
    if (!absl::SimpleAtoi(line.substr(tab + 1), &n) || n < 1 || n > 5) continue;
    leading_strokes_[std::string(line.substr(0, tab))] = n;
  }
}

int IdsTable::LeadingStroke(std::string_view ch) const {
  for (const std::string_view first :
       Utf8AsChars(absl::StripAsciiWhitespace(ch))) {
    return LeadingStrokeAt(first, 0);
  }
  return 0;
}

int IdsTable::LeadingStrokeAt(std::string_view ch, int depth) const {
  if (const auto it = leading_cache_.find(ch); it != leading_cache_.end()) {
    return it->second;
  }
  const std::string key(ch);
  if (const auto it = leading_strokes_.find(key); it != leading_strokes_.end()) {
    return leading_cache_[key] = it->second;
  }
  if (const int builtin = BuiltinLeadingStroke(ch); builtin != 0) {
    return leading_cache_[key] = builtin;
  }
  constexpr int kMaxDepth = 12;
  const auto decompositions = by_char_.find(key);
  if (decompositions == by_char_.end()) return leading_cache_[key] = 0;
  if (depth == 0) leading_cut_short_ = false;
  if (depth >= kMaxDepth || leading_in_progress_.contains(key)) {
    // Cyclic or too deep: unknown here, but only because of where the walk
    // started, so the callers on the way back must not cache their answer.
    leading_cut_short_ = true;
    return 0;
  }
  leading_in_progress_.insert(key);
  int result = 0;
  for (const std::string &ids : decompositions->second) {
    result = LeadingStrokeOfIds(ids, depth + 1);
    if (result != 0) break;
  }
  leading_in_progress_.erase(key);
  if (result == 0 && leading_cut_short_ && depth > 0) return 0;
  return leading_cache_[key] = result;
}

int IdsTable::LeadingStrokeOfIds(std::string_view ids, int depth) const {
  std::vector<std::string_view> chars;
  for (const std::string_view ch : Utf8AsChars(ids)) chars.push_back(ch);
  const size_t n = chars.size();

  // The index just past the subtree that starts at |i|.
  const auto skip = [&](auto &&self, size_t i) -> size_t {
    if (i >= n) return n;
    if (!IsIdsOperator(chars[i])) return i + 1;
    size_t j = i + 1;
    for (int k = PartCountFor(chars[i]); k > 0; --k) j = self(self, j);
    return j;
  };
  // Enclosures whose outer part is written after the inner one.
  const auto written_last = [](std::string_view op, std::string_view part) {
    if (op == "⿺") {
      return part == "辶" || part == "⻌" || part == "⻍" || part == "⻎" ||
             part == "廴";
    }
    return op == "⿶" && part == "凵";
  };
  const auto lead = [&](auto &&self, size_t i) -> int {
    if (i >= n) return 0;
    if (!IsIdsOperator(chars[i])) {
      return IsComponent(chars[i]) ? LeadingStrokeAt(chars[i], depth) : 0;
    }
    const size_t first = i + 1;
    if (first < n && written_last(chars[i], chars[first])) {
      if (const int inner = self(self, skip(skip, first)); inner != 0) {
        return inner;
      }
    }
    return self(self, first);
  };
  return lead(lead, 0);
}

void IdsTable::SortByStrokes(std::vector<std::string> *chars) const {
  struct Keyed {
    int strokes;
    int leading;
    std::string ch;
  };
  std::vector<Keyed> keyed;
  keyed.reserve(chars->size());
  for (std::string &ch : *chars) {
    const std::optional<int> strokes = StrokeCount(ch);
    const int leading = LeadingStroke(ch);
    keyed.push_back({strokes.value_or(std::numeric_limits<int>::max()),
                     leading == 0 ? 6 : leading, std::move(ch)});
  }
  // UTF-8 byte order is code point order.
  std::sort(keyed.begin(), keyed.end(), [](const Keyed &a, const Keyed &b) {
    if (a.strokes != b.strokes) return a.strokes < b.strokes;
    if (a.leading != b.leading) return a.leading < b.leading;
    return a.ch < b.ch;
  });
  chars->clear();
  for (Keyed &k : keyed) chars->push_back(std::move(k.ch));
}

namespace {

// Folds the full-width forms a Japanese IME produces onto ASCII.
std::string NormalizeQuery(std::string_view text) {
  std::string out;
  for (const std::string_view ch : Utf8AsChars(text)) {
    if (ch == "＜") out += '<';
    else if (ch == "＞") out += '>';
    else if (ch == "≤") out += "<=";
    else if (ch == "≥") out += ">=";
    else if (ch == "－" || ch == "−" || ch == "–" || ch == "ー" || ch == "〜" ||
             ch == "～" || ch == "~") out += '-';
    else if (ch.size() == 3 && ch.substr(0, 2) == "\xEF\xBC" &&
             static_cast<unsigned char>(ch[2]) >= 0x90 &&
             static_cast<unsigned char>(ch[2]) <= 0x99) {
      // ０-９
      out += static_cast<char>('0' + (static_cast<unsigned char>(ch[2]) - 0x90));
    } else {
      out.append(ch);
    }
  }
  return out;
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

LookupQuery ParseLookupQuery(std::string_view text) {
  const std::string normalized = NormalizeQuery(text);
  std::vector<std::string_view> chars;
  for (const std::string_view ch : Utf8AsChars(normalized)) chars.push_back(ch);
  const auto ascii_at = [&](size_t i) -> char {
    return i < chars.size() && chars[i].size() == 1 ? chars[i][0] : '\0';
  };

  LookupQuery query;
  for (size_t i = 0; i < chars.size();) {
    const char c = ascii_at(i);
    if (c != '<' && c != '>' && !IsDigit(c)) {
      // A component is one non-ASCII character; ASCII (blanks, letters) is noise.
      if (chars[i].size() > 1) query.components.append(chars[i]);
      ++i;
      continue;
    }
    // A stroke filter: [<|>][=]N or N-M.
    char op = 0;
    bool inclusive = false;
    if (c == '<' || c == '>') {
      op = c;
      ++i;
      if (ascii_at(i) == '=') {
        inclusive = true;
        ++i;
      }
    }
    const auto read_number = [&](int *value) {
      if (!IsDigit(ascii_at(i))) return false;
      int n = 0;
      for (; IsDigit(ascii_at(i)); ++i) {
        n = std::min(n * 10 + (ascii_at(i) - '0'), 999);
      }
      *value = n;
      return true;
    };
    int first = 0;
    if (!read_number(&first)) continue;  // a stray "<" with no number
    StrokeRange range;
    if (op == '<') {
      range.max = inclusive ? first : first - 1;
    } else if (op == '>') {
      range.min = inclusive ? first : first + 1;
    } else if (ascii_at(i) == '-' && IsDigit(ascii_at(i + 1))) {
      ++i;
      int second = 0;
      read_number(&second);
      range.min = std::min(first, second);
      range.max = std::max(first, second);
    } else {
      range.min = range.max = first;
    }
    if (query.strokes.has_value()) {
      // Several filters in one query ("木>3 <9"): all of them apply.
      range.min = std::max(range.min, query.strokes->min);
      range.max = std::min(range.max, query.strokes->max);
    }
    query.strokes = range;
  }
  return query;
}

}  // namespace composer_ids
}  // namespace mozc
