#include "Underground.hpp"
#include <Core/Enum/Game.hpp>
#include <Core/Gen4/Profile4.hpp>
#include <Core/Parents/ProfileLoader.hpp>
#include <Core/Parents/Searchers/SearcherBase.hpp>
#include <Form/Controls/ComboMenu.hpp>
#include <Form/Controls/ComboBox.hpp>
#include <Form/Controls/DateEdit.hpp>
#include <Form/Controls/TableView.hpp>
#include <Form/Controls/TextBox.hpp>
#include <Form/Gen4/Profile/ProfileDisplay4.hpp>
#include <Model/SortFilterProxyModel.hpp>
#include <algorithm>
#include <atomic>
#include <bit>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <QAbstractItemView>
#include <QAbstractTableModel>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPalette>
#include <QPainter>
#include <QPushButton>
#include <QPixmap>
#include <QProgressBar>
#include <QProxyStyle>
#include <QSettings>
#include <QSpinBox>
#include <QStackedLayout>
#include <QStyleFactory>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QTimeEdit>
#include <QTransform>
#include <QVBoxLayout>

namespace
{
constexpr std::array<const char *, 16> plateNames = { "Flame Plate", "Splash Plate", "Zap Plate", "Meadow Plate",
    "Icicle Plate", "Fist Plate", "Toxic Plate", "Earth Plate", "Sky Plate", "Mind Plate", "Insect Plate",
    "Stone Plate", "Spooky Plate", "Draco Plate", "Dread Plate", "Iron Plate" };
const QString settingPrefix = QStringLiteral("underground");
constexpr u8 filterAnyEvoStone = 251;
constexpr u8 filterAnyPlate = 252;
constexpr u8 filterAnyFossil = 253;
constexpr u8 filterAnyShard = 254;
constexpr u8 filterAnyValuable = 255;

class CompactTabBar final : public QTabBar
{
public:
    explicit CompactTabBar(QWidget *parent = nullptr) : QTabBar(parent) {}

    QSize tabSizeHint(int index) const override
    {
        QSize size = QTabBar::tabSizeHint(index);
        size.setWidth(std::max(0, size.width() - 6));
        return size;
    }
};

class CompactTabTextStyle final : public QProxyStyle
{
public:
    explicit CompactTabTextStyle(const QString &style) : QProxyStyle(style) {}

    QRect subElementRect(SubElement element, const QStyleOption *option = nullptr,
                         const QWidget *widget = nullptr) const override
    {
        QRect rect = QProxyStyle::subElementRect(element, option, widget);
        if (element == SE_TabBarTabText) rect.adjust(-3, 0, 3, 0);
        return rect;
    }
};

bool matchesUndergroundFilter(u8 filter, u8 item)
{
    switch (filter)
    {
    case filterAnyEvoStone: return item == 11 || item == 13 || item == 15 || item == 17 || item == 19 || item == 20 || item == 21;
    case filterAnyPlate: return item >= 44 && item <= 59;
    case filterAnyFossil: return (item >= 23 && item <= 27) || item == 36 || item == 37;
    case filterAnyShard: return item >= 31 && item <= 34;
    case filterAnyValuable: return item == 14 || item == 28;
    default: return item == filter;
    }
}

void updateUndergroundFilterCount(QComboBox *filter, QSpinBox *count, int maximum = 4)
{
    const int item = filter->currentData().toInt();
    if (item >= 44 && item <= 59) count->setMaximum(1);
    else if (item == filterAnyPlate) count->setMaximum(std::min(maximum, 16));
    else count->setMaximum(maximum);
    count->setEnabled(item != 0);
}

template <typename Iterator>
int countUndergroundFilter(Iterator begin, Iterator end, u8 filter)
{
    return static_cast<int>(std::count_if(begin, end, [filter](u8 item) { return matchesUndergroundFilter(filter, item); }));
}

void addUndergroundFilterCategories(QComboBox *filter)
{
    filter->addItem(Underground4::tr("Any Evo Stone"), filterAnyEvoStone);
    filter->addItem(Underground4::tr("Any Plate"), filterAnyPlate);
    filter->addItem(Underground4::tr("Any Fossil"), filterAnyFossil);
    filter->addItem(Underground4::tr("Any Shard"), filterAnyShard);
    filter->addItem(Underground4::tr("Any Valuable"), filterAnyValuable);
}

struct UndergroundSprite
{
    u8 x;
    u8 y;
    u8 width;
    u8 height;
    u64 shape;
};

constexpr std::array<UndergroundSprite, 60> undergroundSprites = { {
    {}, { 8, 1, 2, 2, 0xFULL }, { 6, 1, 2, 2, 0xFULL }, { 0, 1, 2, 2, 0xFULL },
    { 4, 1, 2, 2, 0xFULL }, { 2, 1, 2, 2, 0xFULL }, { 3, 3, 3, 3, 0x1FFULL },
    { 0, 3, 3, 3, 0x1FFULL }, { 11, 0, 3, 3, 0x1FFULL }, { 17, 0, 3, 3, 0x1FFULL },
    { 14, 0, 3, 3, 0x1FFULL }, { 7, 6, 3, 3, 0x1FFULL }, { 0, 30, 4, 4, 0xFFFFULL },
    { 0, 6, 3, 3, 0x1FAULL }, { 14, 25, 3, 3, 0xBAULL }, { 3, 6, 4, 2, 0x7EULL },
    { 18, 18, 2, 2, 0xFULL }, { 9, 3, 3, 3, 0xFEULL }, { 10, 6, 4, 2, 0xFFULL },
    { 6, 3, 3, 3, 0x1FFULL }, { 12, 3, 3, 3, 0xFFULL }, { 15, 3, 4, 3, 0x6F6ULL },
    {}, { 9, 18, 4, 4, 0x7FFEULL }, { 13, 18, 5, 4, 0x77FFFULL },
    { 5, 18, 4, 5, 0x37EECULL }, { 0, 18, 5, 5, 0xCC6FEFULL }, { 0, 23, 4, 4, 0x7FFEULL },
    { 14, 28, 6, 3, 0x21FE1ULL }, { 13, 22, 3, 3, 0xBAULL }, { 16, 22, 3, 3, 0x1FFULL },
    { 0, 27, 3, 3, 0x1DFULL }, { 3, 27, 3, 3, 0xFFULL }, { 6, 27, 4, 3, 0xF75ULL },
    { 10, 27, 4, 3, 0xBFFULL }, { 14, 6, 2, 2, 0xDULL }, { 4, 23, 5, 4, 0x77DCEULL },
    { 9, 23, 4, 4, 0x6FFFULL }, { 4, 30, 4, 4, 0xAFF5ULL }, { 17, 25, 3, 3, 0x1FFULL },
    { 8, 30, 4, 4, 0x9FF6ULL }, { 12, 31, 4, 4, 0x2E74ULL }, { 16, 31, 4, 3, 0xFF5ULL },
    { 0, 34, 3, 3, 0x7FULL }, { 16, 6, 4, 3, 0xFFFULL }, { 0, 9, 4, 3, 0xFFFULL },
    { 4, 9, 4, 3, 0xFFFULL }, { 8, 9, 4, 3, 0xFFFULL }, { 12, 9, 4, 3, 0xFFFULL },
    { 16, 9, 4, 3, 0xFFFULL }, { 0, 12, 4, 3, 0xFFFULL }, { 4, 12, 4, 3, 0xFFFULL },
    { 8, 12, 4, 3, 0xFFFULL }, { 12, 12, 4, 3, 0xFFFULL }, { 16, 12, 4, 3, 0xFFFULL },
    { 0, 15, 4, 3, 0xFFFULL }, { 4, 15, 4, 3, 0xFFFULL }, { 8, 15, 4, 3, 0xFFFULL },
    { 12, 15, 4, 3, 0xFFFULL }, { 16, 15, 4, 3, 0xFFFULL }
} };

std::tuple<u8, u8, u64> rotateShape(u8 width, u8 height, u64 shape)
{
    u64 rotated = 0;
    for (u8 y = 0; y < height; y++) for (u8 x = 0; x < width; x++)
    {
        if (shape & (1ULL << (y * width + x))) rotated |= 1ULL << (x * height + height - 1 - y);
    }
    return { height, width, rotated };
}

int spriteRotation(const UndergroundSprite &sprite, const UndergroundObject &object)
{
    u8 width = sprite.width;
    u8 height = sprite.height;
    u64 shape = sprite.shape;
    for (int rotation = 0; rotation < 4; rotation++)
    {
        if (width == object.width && height == object.height && shape == object.shape) return rotation * 90;
        std::tie(width, height, shape) = rotateShape(width, height, shape);
    }
    return 0;
}

struct AdvanceGuidance
{
    QString sequence = QStringLiteral("-");
    QString recovered = QStringLiteral("-");
    int recoveredCount = std::numeric_limits<int>::max();
    bool collapsed = false;
    u8 switches = std::numeric_limits<u8>::max();
    u8 maxRun = 0;
    int strikes = std::numeric_limits<int>::max();
};

struct StrikeState
{
    u16 damage = 0;
    u32 advances = 0;
    QString sequence;
    u8 lastTool = 2;
    u8 switches = 0;
    u8 lastRun = 0;
    u8 maxRun = 0;
    u8 pickaxes = 0;
    u8 hammers = 0;
};

class MiningPlanner
{
public:
    explicit MiningPlanner(const UndergroundResult &result) : result(result)
    {
        for (const auto &object : result.objects)
        {
            for (u8 y = 0; y < object.height; y++) for (u8 x = 0; x < object.width; x++)
            {
                if (!(object.shape & (1ULL << (y * object.width + x)))) continue;
                const int cell = (object.y + y) * 13 + object.x + x;
                if (object.metal) metal[cell] = true;
                else
                {
                    itemCells[object.number - 1].push_back(cell);
                }
            }
        }
    }

    bool canRecover(u8 targetMask, u8 pickaxes, u8 hammers)
    {
        const u32 key = targetMask | (static_cast<u32>(pickaxes) << 8) | (static_cast<u32>(hammers) << 16);
        if (const auto it = combinedAnswers.find(key); it != combinedAnswers.end()) return it->second;
        const bool answer = combine(targetMask, pickaxes, hammers);
        combinedAnswers.emplace(key, answer);
        return answer;
    }

private:
    bool recovered(const std::array<u8, 130> &dirt, u8 item) const
    {
        const auto &object = result.objects[item];
        for (u8 y = 0; y < object.height; y++) for (u8 x = 0; x < object.width; x++)
        {
            if ((object.shape & (1ULL << (y * object.width + x)))
                && dirt[(object.y + y) * 13 + object.x + x] != 0)
                return false;
        }
        return true;
    }

    void hit(std::array<u8, 130> &dirt, int center, bool hammer) const
    {
        const int cx = center % 13;
        const int cy = center / 13;
        dirt[center] = dirt[center] > 2 ? dirt[center] - 2 : 0;
        if (metal[center] && dirt[center] == 0) return;
        static constexpr int dx[8] = { 0, 0, -1, 1, 1, -1, -1, 1 };
        static constexpr int dy[8] = { 1, -1, 0, 0, 1, -1, 1, -1 };
        for (int i = 0; i < (hammer ? 8 : 4); i++)
        {
            const int x = cx + dx[i];
            const int y = cy + dy[i];
            if (x < 0 || x >= 13 || y < 0 || y >= 10) continue;
            u8 &value = dirt[y * 13 + x];
            const u8 amount = hammer && i < 4 ? 2 : 1;
            value = value > amount ? value - amount : 0;
        }
    }

    bool groupCanRecover(u8 targetMask, u8 pickaxes, u8 hammers)
    {
        const u32 key = targetMask | (static_cast<u32>(pickaxes) << 8) | (static_cast<u32>(hammers) << 16);
        if (const auto it = groupAnswers.find(key); it != groupAnswers.end()) return it->second;
        bool dominated = false;
        for (const auto &[knownKey, possible] : groupAnswers)
        {
            if (!possible || static_cast<u8>(knownKey) != targetMask) continue;
            const u8 knownPickaxes = static_cast<u8>(knownKey >> 8);
            const u8 knownHammers = static_cast<u8>(knownKey >> 16);
            if (knownPickaxes <= pickaxes && knownHammers <= hammers)
            {
                dominated = true;
                break;
            }
        }
        if (dominated)
        {
            groupAnswers.emplace(key, true);
            return true;
        }
        const bool answer = greedyRecover(targetMask, pickaxes, hammers, 0)
            || greedyRecover(targetMask, pickaxes, hammers, 1)
            || greedyRecover(targetMask, pickaxes, hammers, 2);
        groupAnswers.emplace(key, answer);
        return answer;
    }

    bool combine(u8 targetMask, u8 pickaxes, u8 hammers)
    {
        if (targetMask == 0) return true;
        const u32 key = targetMask | (static_cast<u32>(pickaxes) << 8) | (static_cast<u32>(hammers) << 16);
        if (const auto it = combineMemo.find(key); it != combineMemo.end()) return it->second;
        const u8 first = static_cast<u8>(std::countr_zero(static_cast<unsigned>(targetMask)));
        const u8 firstBit = static_cast<u8>(1 << first);
        const u8 remaining = static_cast<u8>(targetMask & ~firstBit);
        for (int usedHammers = hammers; usedHammers >= 0; usedHammers--)
        {
            for (int usedPickaxes = pickaxes; usedPickaxes >= 0; usedPickaxes--)
            {
                if (groupCanRecover(firstBit, static_cast<u8>(usedPickaxes), static_cast<u8>(usedHammers))
                    && combine(remaining, static_cast<u8>(pickaxes - usedPickaxes),
                               static_cast<u8>(hammers - usedHammers)))
                {
                    combineMemo.emplace(key, true);
                    return true;
                }
            }
        }
        combineMemo.emplace(key, false);
        return false;
    }

    bool greedyRecover(u8 targetMask, u8 pickaxes, u8 hammers, u8 mode) const
    {
        auto dirt = result.dirt;
        const u8 item = static_cast<u8>(std::countr_zero(static_cast<unsigned>(targetMask)));
        while (pickaxes || hammers)
        {
            if (recovered(dirt, item)) return true;
            struct Choice { int center = -1; bool hammer = false; int score = 0; } best;
            for (int center : itemCells[item])
            {
                for (u8 tool = 0; tool < 2; tool++)
                {
                    const bool hammer = tool == 1;
                    if ((hammer ? hammers : pickaxes) == 0) continue;
                    if ((mode == 0 && pickaxes && hammer) || (mode == 1 && hammers && !hammer)) continue;
                    auto next = dirt;
                    hit(next, center, hammer);
                    int score = 0;
                    for (int cell : itemCells[item])
                    {
                        score += dirt[cell] - next[cell];
                    }
                    if (score > best.score) best = { center, hammer, score };
                }
            }
            if (best.center < 0) return false;
            hit(dirt, best.center, best.hammer);
            if (best.hammer) hammers--;
            else pickaxes--;
        }
        return recovered(dirt, item);
    }

    const UndergroundResult &result;
    std::array<bool, 130> metal = {};
    std::array<std::vector<int>, 4> itemCells;
    std::unordered_map<u32, bool> combinedAnswers;
    std::unordered_map<u32, bool> groupAnswers;
    std::unordered_map<u32, bool> combineMemo;
};

struct RecoveryTransition
{
    u32 advances;
    u8 recoveredMask;
};

struct GuidedRecoveryTransition
{
    u32 advances;
    u8 recoveredMask;
    QString sequence;
    bool collapsed;
    u8 switches;
    u8 strikes;
};

struct MiningOutcome
{
    u32 advances;
    u8 pickaxes;
    u8 hammers;
    bool collapsed;
};

const std::vector<MiningOutcome> &getMiningOutcomes()
{
    static const std::vector<MiningOutcome> outcomes = [] {
        using State = std::tuple<u32, u8, u8>;
        std::array<std::set<State>, 197> states;
        states[0].emplace(0, 2, 0);
        std::set<std::tuple<u32, u8, u8, bool>> unique;
        for (u16 damage = 0; damage < 196; damage++)
        {
            for (const auto &[advances, lastTool, hammers] : states[damage])
            {
                Q_UNUSED(lastTool);
                const u8 pickaxes = static_cast<u8>((damage - 8 * hammers) / 4);
                for (u8 tool = 0; tool < 2; tool++)
                {
                    const u16 nextDamage = std::min<u16>(196, damage + (tool == 1 ? 8 : 4));
                    const u8 nextPickaxes = pickaxes + (tool == 0);
                    const u8 nextHammers = hammers + (tool == 1);
                    const u8 duration = static_cast<u8>(nextDamage / 15);
                    const u32 finalBase = advances + (duration > 1 ? 2 : 0);
                    unique.emplace(finalBase, nextPickaxes, nextHammers, false);
                    if (nextDamage == 196)
                    {
                        unique.emplace(finalBase, nextPickaxes, nextHammers, true);
                        continue;
                    }
                    const u8 maximumShakeFrames = duration > 1 ? duration - 1 : 0;
                    states[nextDamage].emplace(advances + 2 * maximumShakeFrames, tool, nextHammers);
                }
            }
        }
        std::vector<MiningOutcome> values;
        for (const auto &[advances, pickaxes, hammers, collapsed] : unique)
        {
            const bool dominated = std::ranges::any_of(values, [&](const MiningOutcome &value) {
                return value.advances == advances && value.collapsed == collapsed
                    && value.pickaxes >= pickaxes && value.hammers >= hammers;
            });
            if (dominated) continue;
            std::erase_if(values, [&](const MiningOutcome &value) {
                return value.advances == advances && value.collapsed == collapsed
                    && pickaxes >= value.pickaxes && hammers >= value.hammers;
            });
            values.push_back({ advances, pickaxes, hammers, collapsed });
        }
        return values;
    }();
    return outcomes;
}

u8 recoveredSphereCount(const UndergroundResult &result, u8 mask);
QString formatStrikeSequence(const QString &sequence);

struct GuidedMiningOutcome
{
    u32 advances;
    u8 pickaxes;
    u8 hammers;
    QString sequence;
    u8 switches;
    u8 strikes;
    bool collapsed;
};

const std::vector<GuidedMiningOutcome> &getGuidedMiningOutcomes()
{
    static const std::vector<GuidedMiningOutcome> outcomes = [] {
        using StateKey = std::tuple<u32, u8, u8>;
        std::array<std::map<StateKey, StrikeState>, 197> states;
        states[0].emplace(StateKey(0, 2, 0), StrikeState());
        std::map<std::tuple<u32, u8, u8, bool>, GuidedMiningOutcome> best;
        for (u16 damage = 0; damage < 196; damage++)
        {
            for (const auto &[key, state] : states[damage])
            {
                for (u8 tool = 0; tool < 2; tool++)
                {
                    StrikeState next = state;
                    next.damage = std::min<u16>(196, damage + (tool == 1 ? 8 : 4));
                    next.sequence += next.sequence.isEmpty() ? (tool == 1 ? QStringLiteral("H") : QStringLiteral("P"))
                                                            : (tool == 1 ? QStringLiteral(", H") : QStringLiteral(", P"));
                    if (state.lastTool == tool) next.lastRun++;
                    else
                    {
                        if (state.lastTool != 2) next.switches++;
                        next.lastRun = 1;
                    }
                    next.lastTool = tool;
                    if (tool == 1) next.hammers++;
                    else next.pickaxes++;

                    const u8 duration = static_cast<u8>(next.damage / 15);
                    const u32 finalBase = state.advances + (duration > 1 ? 2 : 0);
                    for (bool collapsed : { false, true })
                    {
                        if (collapsed && next.damage != 196) continue;
                        const auto outcomeKey = std::make_tuple(finalBase, next.pickaxes, next.hammers, collapsed);
                        GuidedMiningOutcome candidate { finalBase, next.pickaxes, next.hammers,
                            formatStrikeSequence(next.sequence), next.switches,
                            static_cast<u8>(next.pickaxes + next.hammers), collapsed };
                        const auto it = best.find(outcomeKey);
                        if (it == best.end() || candidate.switches < it->second.switches)
                            best[outcomeKey] = std::move(candidate);
                    }
                    if (next.damage == 196) continue;

                    const u8 maximumShakeFrames = duration > 1 ? duration - 1 : 0;
                    StrikeState continued = next;
                    continued.advances = state.advances + 2 * maximumShakeFrames;
                    const StateKey nextKey(continued.advances, continued.lastTool, continued.hammers);
                    auto [it, inserted] = states[continued.damage].emplace(nextKey, continued);
                    if (!inserted && continued.switches < it->second.switches) it->second = std::move(continued);
                }
            }
        }
        std::vector<GuidedMiningOutcome> values;
        values.reserve(best.size());
        for (auto &[key, outcome] : best) values.emplace_back(std::move(outcome));
        return values;
    }();
    return outcomes;
}

std::vector<GuidedRecoveryTransition> getGuidedRecoveryTransitions(const UndergroundResult &result, MiningPlanner &planner,
                                                                   bool requireAllPlates)
{
    struct Key
    {
        u32 advances;
        u8 mask;
        bool operator<(const Key &other) const { return std::tie(advances, mask) < std::tie(other.advances, other.mask); }
    };
    std::map<Key, GuidedRecoveryTransition> best;
    const u8 allMask = static_cast<u8>((1 << result.itemCount) - 1);
    u8 plateMask = 0;
    for (u8 item = 0; item < result.itemCount; item++)
        if (result.items[item] >= 44 && result.items[item] <= 59) plateMask |= 1 << item;

    for (const GuidedMiningOutcome &outcome : getGuidedMiningOutcomes())
    {
        const u8 firstMask = outcome.collapsed ? 0 : allMask;
        const u8 lastMask = outcome.collapsed ? static_cast<u8>(allMask - 1) : allMask;
        for (u8 mask = firstMask; mask <= lastMask; mask++)
        {
            if (requireAllPlates && (mask & plateMask) != plateMask) continue;
            if (!planner.canRecover(mask, outcome.pickaxes, outcome.hammers)) continue;
            const u32 advances = outcome.advances
                + (outcome.collapsed ? 88 + 6 * std::popcount(mask) + recoveredSphereCount(result, mask)
                                     : 6 * result.itemCount + recoveredSphereCount(result, allMask));
            GuidedRecoveryTransition candidate { advances, mask, outcome.sequence, outcome.collapsed,
                                                   outcome.switches, outcome.strikes };
            const Key key { advances, mask };
            const auto it = best.find(key);
            if (it == best.end() || std::tie(candidate.switches, candidate.strikes)
                    < std::tie(it->second.switches, it->second.strikes))
                best[key] = std::move(candidate);
        }
    }
    std::vector<GuidedRecoveryTransition> transitions;
    transitions.reserve(best.size());
    for (auto &[key, transition] : best) transitions.emplace_back(std::move(transition));
    return transitions;
}

u8 recoveredSphereCount(const UndergroundResult &result, u8 mask)
{
    u8 count = 0;
    for (u8 item = 0; item < result.itemCount; item++)
        if ((mask & (1 << item)) && result.items[item] >= 1 && result.items[item] <= 10) count++;
    return count;
}

std::pair<u32, u32> getPotentialAdvanceRange(const UndergroundResult &result)
{
    u32 minimum = std::numeric_limits<u32>::max();
    u32 maximum = 0;
    const u8 allMask = static_cast<u8>((1 << result.itemCount) - 1);
    const u8 spheres = recoveredSphereCount(result, allMask);
    for (const MiningOutcome &outcome : getMiningOutcomes())
    {
        if (!outcome.collapsed)
        {
            const u32 advances = outcome.advances + 6 * result.itemCount + spheres;
            minimum = std::min(minimum, advances);
            maximum = std::max(maximum, advances);
        }
        else for (u8 mask = 0; mask < allMask; mask++)
        {
            const u32 advances
                = outcome.advances + 88 + 6 * std::popcount(mask) + recoveredSphereCount(result, mask);
            minimum = std::min(minimum, advances);
            maximum = std::max(maximum, advances);
        }
    }
    return { minimum, maximum };
}

std::vector<RecoveryTransition> getRecoveryTransitions(const UndergroundResult &result, MiningPlanner &planner,
                                                       const std::vector<bool> *allowedOffsets = nullptr,
                                                       u16 allowedMasks = std::numeric_limits<u16>::max())
{
    const auto [minimumAdvance, maximumAdvance] = getPotentialAdvanceRange(result);
    Q_UNUSED(minimumAdvance);
    std::vector<u16> transitions(maximumAdvance + 1);
    const u8 allMask = static_cast<u8>((1 << result.itemCount) - 1);
    const u8 spheres = recoveredSphereCount(result, allMask);
    for (const MiningOutcome &outcome : getMiningOutcomes())
    {
        if (!outcome.collapsed)
        {
            const u32 advances = outcome.advances + 6 * result.itemCount + spheres;
            if ((allowedMasks & (1 << allMask))
                && (!allowedOffsets || (advances < allowedOffsets->size() && (*allowedOffsets)[advances]))
                && planner.canRecover(allMask, outcome.pickaxes, outcome.hammers))
                transitions[advances] |= 1 << allMask;
            continue;
        }
        for (u8 mask = 0; mask < allMask; mask++)
        {
            if (!(allowedMasks & (1 << mask))) continue;
            const u32 advances
                = outcome.advances + 88 + 6 * std::popcount(mask) + recoveredSphereCount(result, mask);
            if (allowedOffsets && (advances >= allowedOffsets->size() || !(*allowedOffsets)[advances])) continue;
            if (planner.canRecover(mask, outcome.pickaxes, outcome.hammers)) transitions[advances] |= 1 << mask;
        }
    }

    std::vector<RecoveryTransition> values;
    for (u32 advances = 0; advances < transitions.size(); advances++)
        for (u8 mask = 0; mask < 16; mask++)
            if (transitions[advances] & (1 << mask)) values.push_back({ advances, mask });
    return values;
}

bool betterGuidance(const AdvanceGuidance &candidate, const AdvanceGuidance &current, bool preferCollapse)
{
    const bool candidatePreferred = candidate.collapsed == preferCollapse;
    const bool currentPreferred = current.collapsed == preferCollapse;
    if (candidatePreferred != currentPreferred) return candidatePreferred;
    if (candidate.recoveredCount != current.recoveredCount) return candidate.recoveredCount < current.recoveredCount;
    if (candidate.switches != current.switches) return candidate.switches < current.switches;
    if (candidate.maxRun != current.maxRun) return candidate.maxRun > current.maxRun;
    return candidate.strikes < current.strikes;
}

QString formatStrikeSequence(const QString &sequence)
{
    const QStringList values = sequence.split(',', Qt::SkipEmptyParts);
    QStringList runs;
    for (int index = 0; index < values.size();)
    {
        const QString tool = values[index].trimmed();
        int end = index + 1;
        while (end < values.size() && values[end].trimmed() == tool) end++;
        const int count = end - index;
        runs.append(count == 1 ? tool : QStringLiteral("%1 %2").arg(count).arg(tool));
        index = end;
    }
    return runs.join(QStringLiteral(", "));
}

void updateItemNames(QLabel *label, const UndergroundResult &result)
{
    QStringList names;
    for (u8 item = 0; item < result.itemCount; item++)
    {
        names.append(QStringLiteral("%1: %2")
                         .arg(item + 1)
                         .arg(QString::fromStdString(UndergroundGenerator::getItemName(result.items[item]))));
    }
    label->setText(names.join(QStringLiteral("   ")));
}
}

struct UndergroundAdvanceRow
{
    u32 advance;
    u32 actualAdvance;
    QString sequence;
    QString recovered;
    QString outcome;
    bool collapsed;
    std::array<QString, 4> items;
    std::array<u8, 4> itemIDs;
    u8 itemCount;
};

class UndergroundAdvanceModel final : public QAbstractTableModel
{
public:
    explicit UndergroundAdvanceModel(QObject *parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex & = QModelIndex()) const override { return static_cast<int>(rows.size()); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 8; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const auto &row = rows[index.row()];
        switch (index.column())
        {
        case 0: return row.advance;
        case 1: return row.sequence;
        case 2: return row.recovered;
        case 3: return row.outcome;
        default: return row.items[index.column() - 4];
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const std::array<QString, 8> headers = { tr("Advance"), tr("Hit Sequence"), tr("Recovered"), tr("Outcome"),
            tr("Item 1"), tr("Item 2"), tr("Item 3"), tr("Item 4") };
        return headers[section];
    }
    u32 getActualAdvance(int row) const { return rows[row].actualAdvance; }
    int getRowForActualAdvance(u32 advance) const
    {
        const auto it = std::ranges::find(rows, advance, &UndergroundAdvanceRow::actualAdvance);
        return it == rows.end() ? -1 : static_cast<int>(std::distance(rows.begin(), it));
    }
    bool isCollapsed(int row) const { return rows[row].collapsed; }
    void setAllRows(std::vector<UndergroundAdvanceRow> values)
    {
        allRows = std::move(values);
    }
    void applyFilters(const std::array<std::pair<u8, u8>, 4> &filters, bool onlyTarget = false, u32 target = 0)
    {
        beginResetModel();
        rows.clear();
        u32 visibleAdvance = 0;
        for (const UndergroundAdvanceRow &source : allRows)
        {
            bool matches = true;
            for (const auto &[item, minimum] : filters)
            {
                if (item == 0) continue;
                const int count = countUndergroundFilter(source.itemIDs.begin(), source.itemIDs.begin() + source.itemCount, item);
                if (count < minimum)
                {
                    matches = false;
                    break;
                }
            }
            if (!matches) continue;
            const u32 advance = visibleAdvance++;
            if (onlyTarget && source.actualAdvance != target) continue;
            UndergroundAdvanceRow row = source;
            row.advance = advance;
            rows.emplace_back(std::move(row));
        }
        endResetModel();
    }
private:
    std::vector<UndergroundAdvanceRow> allRows;
    std::vector<UndergroundAdvanceRow> rows;
};

struct UndergroundFinderRow
{
    u64 seed;
    QDate date;
    QTime time;
    u32 delay;
};

class UndergroundFinderModel final : public QAbstractTableModel
{
public:
    explicit UndergroundFinderModel(QObject *parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex & = QModelIndex()) const override { return static_cast<int>(rows.size()); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 4; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const auto &row = rows[index.row()];
        switch (index.column())
        {
        case 0: return QString::number(row.seed, 16).toUpper();
        case 1: return row.date.toString(QStringLiteral("yyyy-MM-dd"));
        case 2: return row.time.toString(QStringLiteral("HH:mm:ss"));
        default: return row.delay;
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const std::array<QString, 4> headers = { tr("Seed"), tr("Date"), tr("Time"), tr("Delay") };
        return headers[section];
    }
    void setRows(std::vector<UndergroundFinderRow> values)
    {
        beginResetModel();
        rows = std::move(values);
        endResetModel();
    }
private:
    std::vector<UndergroundFinderRow> rows;
};

struct UndergroundSearcherRow
{
    u64 seed;
    u32 delay;
    std::array<u8, 4> items;
    u8 itemCount;
    u8 matchingItems;
    u8 walls;
    std::vector<u32> routeAdvances;
    std::vector<u16> routePlateMasks;
};

struct UndergroundRouteMatch
{
    u8 matchingItems;
    u8 walls;
    std::vector<u32> advances;
    std::vector<u16> plateMasks;
};

class UndergroundSearcherModel final : public QAbstractTableModel
{
public:
    explicit UndergroundSearcherModel(QObject *parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex & = QModelIndex()) const override { return static_cast<int>(rows.size()); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 8; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const auto &row = rows[index.row()];
        if (index.column() == 0) return QString::number(row.seed, 16).toUpper();
        if (index.column() == 1) return row.delay;
        if (index.column() == 6) return row.matchingItems;
        if (index.column() == 7) return row.walls;
        const int item = index.column() - 2;
        return item < row.itemCount ? QString::fromStdString(UndergroundGenerator::getItemName(row.items[item])) : QStringLiteral("-");
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const std::array<QString, 8> headers = { tr("Seed"), tr("Delay"), tr("Item 1"), tr("Item 2"), tr("Item 3"),
            tr("Item 4"), tr("Items"), tr("Spots") };
        return headers[section];
    }
    const UndergroundSearcherRow &getRow(int row) const { return rows[row]; }
    void setRows(std::vector<UndergroundSearcherRow> values)
    {
        beginResetModel();
        rows = std::move(values);
        endResetModel();
    }
    void addRows(std::vector<UndergroundSearcherRow> values)
    {
        if (values.empty()) return;
        const int first = static_cast<int>(rows.size());
        beginInsertRows(QModelIndex(), first, first + static_cast<int>(values.size()) - 1);
        rows.insert(rows.end(), std::make_move_iterator(values.begin()), std::make_move_iterator(values.end()));
        endInsertRows();
    }
private:
    std::vector<UndergroundSearcherRow> rows;
};

struct UndergroundChainerRow
{
    u8 spot;
    u32 targetAdvance;
    u32 actualAdvance;
    QString sequence;
    QString recovered;
    QString outcome;
    std::array<QString, 4> items;
    UndergroundResult result;
    u64 spotSeed;
    u16 platesBefore;
};

struct UndergroundChainerResult
{
    std::vector<UndergroundChainerRow> rows;
    u8 recovered = 0;
};

class UndergroundChainerModel final : public QAbstractTableModel
{
public:
    explicit UndergroundChainerModel(QObject *parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex & = QModelIndex()) const override { return static_cast<int>(rows.size()); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 9; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const auto &row = rows[index.row()];
        switch (index.column())
        {
        case 0: return row.spot;
        case 1: return row.targetAdvance;
        case 2: return row.sequence;
        case 3: return row.recovered;
        case 4: return row.outcome;
        default: return row.items[index.column() - 5];
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const std::array<QString, 9> headers = { tr("Spot"), tr("Target Advance"), tr("Hit Sequence"),
            tr("Recovered"), tr("Outcome"), tr("Item 1"), tr("Item 2"), tr("Item 3"), tr("Item 4") };
        return headers[section];
    }
    const UndergroundChainerRow &getRow(int row) const { return rows[row]; }
    void setRows(std::vector<UndergroundChainerRow> values)
    {
        beginResetModel();
        rows = std::move(values);
        endResetModel();
    }
private:
    std::vector<UndergroundChainerRow> rows;
};

class UndergroundChainerSearcher final : public SearcherBase<UndergroundChainerResult>
{
public:
    UndergroundChainerSearcher(u64 seed, u8 target, u8 spots, bool recoverAllPlates, u16 startingPlates,
                               UndergroundGenerator generator) :
        seed(seed), target(target), spots(spots), recoverAllPlates(recoverAllPlates), startingPlates(startingPlates),
        generator(std::move(generator))
    {
    }

    void start()
    {
        setMaxProgress(10000);
        activeThreads.store(1);
        threadContainer.emplace_back([this] {
            run();
            activeThreads.fetch_sub(1);
        });
    }

    int getDetailedProgress() const { return static_cast<int>(progress.load(std::memory_order_relaxed)); }

private:
    struct State
    {
        u32 advance = 0;
        u16 plates = 0;
        u8 recovered = 0;
        u16 switches = 0;
        u16 hits = 0;
        u32 displayAdvance = 0;
        std::vector<UndergroundChainerRow> rows;
    };

    static bool better(const State &left, const State &right)
    {
        if (left.recovered != right.recovered) return left.recovered > right.recovered;
        if (left.switches != right.switches) return left.switches < right.switches;
        if (left.hits != right.hits) return left.hits < right.hits;
        return left.advance < right.advance;
    }

    void run()
    {
        using Key = std::pair<u32, u16>;
        std::map<u16, UndergroundGenerator> generators;
        auto getGenerator = [&](u16 plates) -> UndergroundGenerator & {
            if (const auto it = generators.find(plates); it != generators.end()) return it->second;
            UndergroundGenerator value = generator;
            for (u8 plate = 0; plate < 16; plate++)
                if (plates & (1 << plate)) value.setMinedPlate(44 + plate);
            return generators.emplace(plates, std::move(value)).first->second;
        };

        std::map<Key, State> current;
        current.emplace(Key(0, startingPlates), State { 0, startingPlates });
        for (u8 spot = 0; spot < spots && !cancelled.load(std::memory_order_relaxed); spot++)
        {
            std::map<Key, State> next;
            size_t processed = 0;
            for (const auto &[key, state] : current)
            {
                UndergroundGenerator &routeGenerator = getGenerator(state.plates);
                const u64 spotSeed = UndergroundGenerator::advanceSeed(seed, state.advance);
                const UndergroundResult result = routeGenerator.generate(spotSeed);
                MiningPlanner planner(result);
                const auto transitions = getGuidedRecoveryTransitions(result, planner, false);
                u8 plateItemMask = 0;
                for (u8 item = 0; item < result.itemCount; item++)
                    if (result.items[item] >= 44 && result.items[item] <= 59) plateItemMask |= 1 << item;
                u32 displayAdvance = 0;
                u32 previousAdvance = std::numeric_limits<u32>::max();
                for (const GuidedRecoveryTransition &transition : transitions)
                {
                    if (cancelled.load(std::memory_order_relaxed)) return;
                    if (transition.advances != previousAdvance)
                    {
                        if (previousAdvance != std::numeric_limits<u32>::max()) displayAdvance++;
                        previousAdvance = transition.advances;
                    }
                    if (recoverAllPlates && (transition.recoveredMask & plateItemMask) != plateItemMask) continue;
                    State candidate = state;
                    candidate.advance += result.advances + transition.advances;
                    candidate.switches += transition.switches;
                    candidate.hits += transition.strikes;
                    QStringList recoveredItems;
                    for (u8 item = 0; item < result.itemCount; item++)
                    {
                        if (!(transition.recoveredMask & (1 << item))) continue;
                        recoveredItems.append(QString::number(item + 1));
                        const u8 itemID = result.items[item];
                        if (matchesUndergroundFilter(target, itemID)) candidate.recovered++;
                        if (itemID >= 44 && itemID <= 59
                            && (recoverAllPlates || target == filterAnyPlate || target == itemID))
                            candidate.plates |= static_cast<u16>(1 << (itemID - 44));
                    }
                    UndergroundChainerRow row { static_cast<u8>(spot + 1), state.displayAdvance, state.advance,
                        transition.sequence,
                        recoveredItems.isEmpty() ? QStringLiteral("-") : recoveredItems.join(QStringLiteral(", ")),
                        transition.collapsed ? Underground4::tr("Wall Collapsed") : Underground4::tr("All Item Recovered"),
                        {}, result, spotSeed, state.plates };
                    for (u8 item = 0; item < 4; item++)
                        row.items[item] = item < result.itemCount
                            ? QString::fromStdString(UndergroundGenerator::getItemName(result.items[item])) : QStringLiteral("-");
                    candidate.rows.push_back(std::move(row));
                    candidate.displayAdvance = displayAdvance;
                    const Key nextKey(candidate.advance, candidate.plates);
                    const auto it = next.find(nextKey);
                    if (it == next.end() || better(candidate, it->second)) next[nextKey] = std::move(candidate);
                }
                processed++;
                progress.store((static_cast<u64>(spot) * 10000
                                    + static_cast<u64>(processed) * 10000 / current.size()) / spots,
                               std::memory_order_relaxed);
            }
            current = std::move(next);
        }
        if (cancelled.load(std::memory_order_relaxed) || current.empty()) return;
        const State *best = nullptr;
        for (const auto &[key, state] : current)
            if (!best || better(state, *best)) best = &state;
        std::lock_guard<std::mutex> guard(mutex);
        results.push_back({ best->rows, best->recovered });
    }

    u64 seed;
    u8 target;
    u8 spots;
    bool recoverAllPlates;
    u16 startingPlates;
    UndergroundGenerator generator;
};

class UndergroundSeedSearcher final : public SearcherBase<UndergroundSearcherRow>
{
public:
    UndergroundSeedSearcher(QDate startDate, QDate endDate, u32 minDelay, u32 maxDelay,
                            std::array<std::pair<u8, u8>, 4> filters, u8 maxWalls, bool recoverAllPlates,
                            UndergroundGenerator generator) :
        startDate(startDate), endDate(endDate), minDelay(minDelay), maxDelay(maxDelay), filters(filters), maxWalls(maxWalls),
        recoverAllPlates(recoverAllPlates), generator(generator)
    {
    }

    void start()
    {
        setMaxProgress(static_cast<u64>(startDate.daysTo(endDate) + 1) * 1000);
        activeThreads.store(1);
        threadContainer.emplace_back([this] {
            run();
            activeThreads.fetch_sub(1);
        });
    }

    int getDetailedProgress() const
    {
        return static_cast<int>((progress.load(std::memory_order_relaxed) * 10000) / maxProgress);
    }

private:
    std::vector<UndergroundRouteMatch> findRoutes(u64 seed, const UndergroundResult &initial)
    {
        bool hasFilter = false;
        bool anyPlateTarget = false;
        u16 targetedPlates = 0;
        for (const auto &[item, minimum] : filters)
        {
            if (item != 0 && minimum != 0) hasFilter = true;
            if (item != 0 && minimum > 4 * maxWalls) return {};
            if (item == filterAnyPlate) anyPlateTarget = true;
            else if (item >= 44 && item <= 59) targetedPlates |= static_cast<u16>(1 << (item - 44));
        }
        if (!hasFilter) return { { 0, 1, {}, {} } };

        struct RouteState
        {
            u32 advance;
            u16 minedPlates;
            std::array<u8, 4> counts;
            u8 matchingItems;
            std::array<u32, 7> route;
            std::array<u16, 7> routePlateMasks;
            u8 routeLength;
        };
        struct RouteKey
        {
            u32 advance;
            u16 minedPlates;
            std::array<u8, 4> counts;
            u8 matchingItems;
            std::array<u32, 7> route;
            u8 routeLength;
            bool operator==(const RouteKey &) const = default;
        };
        struct RouteKeyHash
        {
            size_t operator()(const RouteKey &key) const
            {
                size_t value = key.advance;
                value = value * 31 + key.minedPlates;
                for (u8 count : key.counts) value = value * 31 + count;
                value = value * 31 + key.matchingItems;
                for (u8 index = 0; index < key.routeLength; index++) value = value * 31 + key.route[index];
                value = value * 31 + key.routeLength;
                return value;
            }
        };
        using WallKey = std::pair<u32, u16>;
        std::map<u16, UndergroundGenerator> generatorCache;
        generatorCache.emplace(0, generator);
        auto getRouteGenerator = [&](u16 minedPlates) -> const UndergroundGenerator & {
            if (const auto it = generatorCache.find(minedPlates); it != generatorCache.end()) return it->second;
            UndergroundGenerator routeGenerator = generator;
            for (u8 plate = 0; plate < 16; plate++)
                if (minedPlates & (1 << plate)) routeGenerator.setMinedPlate(44 + plate);
            return generatorCache.emplace(minedPlates, std::move(routeGenerator)).first->second;
        };
        std::map<WallKey, UndergroundResult> wallCache;
        wallCache.emplace(WallKey(0, 0), initial);
        auto getWall = [&](u32 advance, u16 minedPlates) -> const UndergroundResult & {
            const WallKey key(advance, minedPlates);
            if (const auto it = wallCache.find(key); it != wallCache.end()) return it->second;
            return wallCache
                .emplace(key, getRouteGenerator(minedPlates).generate(UndergroundGenerator::advanceSeed(seed, advance)))
                .first->second;
        };
        std::map<WallKey, std::unique_ptr<MiningPlanner>> plannerCache;
        std::map<WallKey, UndergroundItemResult> itemCache;
        auto getItems = [&](u32 advance, u16 minedPlates, u64 advancedSeed) -> const UndergroundItemResult & {
            const WallKey key(advance, minedPlates);
            if (const auto it = itemCache.find(key); it != itemCache.end()) return it->second;
            if (const auto it = wallCache.find(key); it != wallCache.end())
                return itemCache.emplace(key, UndergroundItemResult { it->second.items, it->second.itemCount }).first->second;
            return itemCache.emplace(key, getRouteGenerator(minedPlates).generateItems(advancedSeed)).first->second;
        };
        std::vector<RouteState> current = { { 0, 0, {}, 0, {}, {}, 0 } };
        for (u8 wall = 1; wall <= maxWalls && !cancelled.load(std::memory_order_relaxed); wall++)
        {
            std::vector<RouteState> next;
            std::unordered_set<RouteKey, RouteKeyHash> seen;
            std::vector<UndergroundRouteMatch> matches;
            std::set<std::pair<std::vector<u32>, u8>> uniqueMatches;
            for (const RouteState &state : current)
            {
                const UndergroundResult &result = getWall(state.advance, state.minedPlates);
                bool canStillComplete = true;
                for (size_t filter = 0; filter < filters.size(); filter++)
                {
                    if (filters[filter].first == 0) continue;
                    const u8 onThisWall = static_cast<u8>(countUndergroundFilter(
                        result.items.begin(), result.items.begin() + result.itemCount, filters[filter].first));
                    const u8 futureMaximum = static_cast<u8>(4 * (maxWalls - wall));
                    if (state.counts[filter] + onThisWall + futureMaximum < filters[filter].second)
                    {
                        canStillComplete = false;
                        break;
                    }
                }
                if (!canStillComplete) continue;

                const u8 allMask = static_cast<u8>((1 << result.itemCount) - 1);
                u8 plateItemMask = 0;
                for (u8 item = 0; item < result.itemCount; item++)
                    if (result.items[item] >= 44 && result.items[item] <= 59) plateItemMask |= 1 << item;
                u16 allowedMasks = 0;
                for (u8 mask = 0; mask <= allMask; mask++)
                {
                    bool useful = !recoverAllPlates || (mask & plateItemMask) == plateItemMask;
                    for (size_t filter = 0; filter < filters.size(); filter++)
                    {
                        if (filters[filter].first == 0) continue;
                        u8 recovered = 0;
                        for (u8 item = 0; item < result.itemCount; item++)
                            if ((mask & (1 << item)) && matchesUndergroundFilter(filters[filter].first, result.items[item]))
                                recovered++;
                        if (state.counts[filter] + recovered + 4 * (maxWalls - wall) < filters[filter].second)
                        {
                            useful = false;
                            break;
                        }
                    }
                    if (useful) allowedMasks |= 1 << mask;
                }
                if (allowedMasks == 0) continue;

                const auto [minimumOffset, maximumOffset] = getPotentialAdvanceRange(result);
                std::vector<bool> allowedOffsets(maximumOffset + 1);
                if (wall < maxWalls)
                {
                    u16 mandatoryPlateMask = 0;
                    std::vector<u16> possiblePlateMasks = { 0 };
                    for (u8 item = 0; item < result.itemCount; item++)
                    {
                        if (result.items[item] < 44 || result.items[item] > 59) continue;
                        const u16 plate = static_cast<u16>(1 << (result.items[item] - 44));
                        if (recoverAllPlates)
                        {
                            mandatoryPlateMask |= plate;
                            continue;
                        }
                        if (!anyPlateTarget && !(targetedPlates & plate)) continue;
                        const size_t size = possiblePlateMasks.size();
                        for (size_t i = 0; i < size; i++) possiblePlateMasks.push_back(possiblePlateMasks[i] | plate);
                    }
                    if (recoverAllPlates) possiblePlateMasks = { mandatoryPlateMask };
                    std::ranges::sort(possiblePlateMasks);
                    possiblePlateMasks.erase(std::unique(possiblePlateMasks.begin(), possiblePlateMasks.end()),
                                             possiblePlateMasks.end());

                    std::array<u8, 4> currentMaximum = {};
                    for (size_t filter = 0; filter < filters.size(); filter++)
                        if (filters[filter].first != 0)
                            currentMaximum[filter] = static_cast<u8>(countUndergroundFilter(
                                result.items.begin(), result.items.begin() + result.itemCount, filters[filter].first));
                    u64 advancedSeed
                        = UndergroundGenerator::advanceSeed(seed, state.advance + result.advances + minimumOffset);
                    for (u32 offset = minimumOffset;; offset++)
                    {
                        const u32 nextAdvance = state.advance + result.advances + offset;
                        bool promising = false;
                        for (u16 addedPlates : possiblePlateMasks)
                        {
                            const UndergroundItemResult &nextWall
                                = getItems(nextAdvance, state.minedPlates | addedPlates, advancedSeed);
                            promising = true;
                            for (size_t filter = 0; filter < filters.size(); filter++)
                            {
                                if (filters[filter].first == 0) continue;
                                const u8 nextMaximum = static_cast<u8>(countUndergroundFilter(
                                    nextWall.items.begin(), nextWall.items.begin() + nextWall.itemCount, filters[filter].first));
                                if (state.counts[filter] + currentMaximum[filter] + nextMaximum
                                        + 4 * (maxWalls - wall - 1)
                                    < filters[filter].second)
                                {
                                    promising = false;
                                    break;
                                }
                            }
                            if (promising) break;
                        }
                        if (promising) allowedOffsets[offset] = true;
                        if (offset == maximumOffset) break;
                        advancedSeed = UndergroundGenerator::nextSeed(advancedSeed);
                    }
                    if (std::ranges::none_of(allowedOffsets, [](bool allowed) { return allowed; })) continue;
                }

                const WallKey wallKey(state.advance, state.minedPlates);
                auto &planner = plannerCache[wallKey];
                if (!planner) planner = std::make_unique<MiningPlanner>(result);
                const auto transitions = getRecoveryTransitions(
                    result, *planner, wall < maxWalls ? &allowedOffsets : nullptr, allowedMasks);
                for (const RecoveryTransition &transition : transitions)
                {
                    if (cancelled.load(std::memory_order_relaxed)) return {};
                    RouteState candidate = state;
                    candidate.advance = state.advance + result.advances + transition.advances;
                    for (u8 itemIndex = 0; itemIndex < result.itemCount; itemIndex++)
                    {
                        if (!(transition.recoveredMask & (1 << itemIndex))) continue;
                        const u8 item = result.items[itemIndex];
                        bool matchesAny = false;
                        for (size_t filter = 0; filter < filters.size(); filter++)
                        {
                            if (filters[filter].first == 0 || !matchesUndergroundFilter(filters[filter].first, item)) continue;
                            candidate.counts[filter] = std::min<u8>(filters[filter].second, candidate.counts[filter] + 1);
                            matchesAny = true;
                        }
                        if (matchesAny) candidate.matchingItems++;
                        if (item >= 44 && item <= 59)
                        {
                            const u16 plate = static_cast<u16>(1 << (item - 44));
                            if (recoverAllPlates || anyPlateTarget || (targetedPlates & plate)) candidate.minedPlates |= plate;
                        }
                    }
                    bool complete = true;
                    for (size_t filter = 0; filter < filters.size(); filter++)
                    {
                        if (filters[filter].first != 0 && candidate.counts[filter] < filters[filter].second)
                        {
                            complete = false;
                            break;
                        }
                    }
                    if (complete)
                    {
                        std::vector<u32> route(state.route.begin(), state.route.begin() + state.routeLength);
                        std::vector<u16> plateMasks(
                            state.routePlateMasks.begin(), state.routePlateMasks.begin() + state.routeLength);
                        if (uniqueMatches.emplace(route, candidate.matchingItems).second)
                            matches.push_back(
                                { candidate.matchingItems, wall, std::move(route), std::move(plateMasks) });
                        continue;
                    }
                    if (wall == maxWalls) continue;
                    candidate.route[candidate.routeLength] = candidate.advance;
                    candidate.routePlateMasks[candidate.routeLength] = candidate.minedPlates;
                    candidate.routeLength++;
                    const RouteKey key { candidate.advance, candidate.minedPlates, candidate.counts, candidate.matchingItems,
                                         candidate.route, candidate.routeLength };
                    if (seen.insert(key).second) next.emplace_back(std::move(candidate));
                }
            }
            if (!matches.empty()) return matches;
            current = std::move(next);
        }
        return {};
    }

    void run()
    {
        u64 lastCheckedSeed = 0;
        bool haveLastCheckedSeed = false;
        for (QDate date = startDate; date <= endDate && !cancelled.load(std::memory_order_relaxed); date = date.addDays(1))
        {
            const u64 dateIndex = static_cast<u64>(startDate.daysTo(date));
            std::vector<u64> bases;
            bases.reserve(24 * 60 * 60);
            for (int hour = 0; hour < 24; hour++) for (int minute = 0; minute < 60; minute++) for (int second = 0; second < 60; second++)
            {
                u64 base = static_cast<u64>(date.year() % 100);
                base = base * 16 + date.month();
                base = base * 32 + date.day();
                base = base * 32 + hour;
                base = base * 64 + minute;
                base = base * 64 + second;
                bases.push_back(base);
            }

            const u64 candidateSpan = bases.back() + maxDelay - (bases.front() + minDelay) + 1;
            std::atomic<u64> processed = 0;
            auto testRange = [&](u64 first, u64 last) {
                if (haveLastCheckedSeed && first <= lastCheckedSeed) first = lastCheckedSeed + 1;
                if (first > last) return;
                std::atomic<u64> nextSeed = first;
                const u64 threadCount = std::min<u64>(std::max(1u, std::thread::hardware_concurrency()), last - first + 1);
                std::vector<std::thread> workers;
                workers.reserve(static_cast<size_t>(threadCount));
                for (u64 thread = 0; thread < threadCount; thread++)
                {
                    workers.emplace_back([&, first, last] {
                        std::vector<UndergroundSearcherRow> found;
                        auto flush = [&] {
                            if (found.empty()) return;
                            std::lock_guard<std::mutex> guard(mutex);
                            results.insert(results.end(), std::make_move_iterator(found.begin()),
                                           std::make_move_iterator(found.end()));
                            found.clear();
                        };
                        while (!cancelled.load(std::memory_order_relaxed))
                        {
                            const u64 chunkStart = nextSeed.fetch_add(32, std::memory_order_relaxed);
                            if (chunkStart > last) break;
                            const u64 chunkEnd = std::min<u64>(last, chunkStart + 31);
                            for (u64 candidateSeed = chunkStart; candidateSeed <= chunkEnd; candidateSeed++)
                            {
                                const UndergroundResult result = generator.generate(candidateSeed);
                                std::vector<UndergroundRouteMatch> routes = findRoutes(candidateSeed, result);
                                if (!routes.empty())
                                {
                                    const u64 minimumBase = candidateSeed >= maxDelay ? candidateSeed - maxDelay : 0;
                                    const u64 maximumBase = candidateSeed >= minDelay ? candidateSeed - minDelay : 0;
                                    auto base = std::upper_bound(bases.begin(), bases.end(), maximumBase);
                                    if (base != bases.begin())
                                    {
                                        --base;
                                        if (*base >= minimumBase)
                                        {
                                            for (UndergroundRouteMatch &route : routes)
                                                found.push_back({ candidateSeed, static_cast<u32>(candidateSeed - *base), result.items,
                                                                  result.itemCount, route.matchingItems, route.walls,
                                                                  std::move(route.advances), std::move(route.plateMasks) });
                                            flush();
                                        }
                                    }
                                }
                                const u64 done = processed.fetch_add(1, std::memory_order_relaxed) + 1;
                                progress.store(dateIndex * 1000 + std::min<u64>(999, (done * 1000) / candidateSpan),
                                               std::memory_order_relaxed);
                                if (cancelled.load(std::memory_order_relaxed) || candidateSeed == chunkEnd) break;
                            }
                            if (found.size() >= 64) flush();
                        }
                        flush();
                    });
                }
                for (std::thread &worker : workers) worker.join();
                if (!haveLastCheckedSeed || last > lastCheckedSeed)
                {
                    lastCheckedSeed = last;
                    haveLastCheckedSeed = true;
                }
            };

            u64 rangeStart = bases.front() + minDelay;
            u64 rangeEnd = bases.front() + maxDelay;
            for (size_t i = 1; i < bases.size() && !cancelled.load(std::memory_order_relaxed); i++)
            {
                const u64 start = bases[i] + minDelay;
                const u64 end = bases[i] + maxDelay;
                if (start <= rangeEnd + 1) rangeEnd = std::max(rangeEnd, end);
                else
                {
                    testRange(rangeStart, rangeEnd);
                    rangeStart = start;
                    rangeEnd = end;
                }
            }
            if (!cancelled.load(std::memory_order_relaxed)) testRange(rangeStart, rangeEnd);
            if (!cancelled.load(std::memory_order_relaxed))
                progress.store((dateIndex + 1) * 1000, std::memory_order_relaxed);
        }
    }

    QDate startDate;
    QDate endDate;
    u32 minDelay;
    u32 maxDelay;
    std::array<std::pair<u8, u8>, 4> filters;
    u8 maxWalls;
    bool recoverAllPlates;
    UndergroundGenerator generator;
};

struct UndergroundTimeRow
{
    QDate date;
    QTime time;
    u32 delay;
};

class UndergroundTimeModel final : public QAbstractTableModel
{
public:
    explicit UndergroundTimeModel(QObject *parent) : QAbstractTableModel(parent) {}
    int rowCount(const QModelIndex & = QModelIndex()) const override { return static_cast<int>(rows.size()); }
    int columnCount(const QModelIndex & = QModelIndex()) const override { return 3; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) return {};
        const auto &row = rows[index.row()];
        if (index.column() == 0) return row.date.toString(QStringLiteral("yyyy-MM-dd"));
        if (index.column() == 1) return row.time.toString(QStringLiteral("HH:mm:ss"));
        return row.delay;
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        static const std::array<QString, 3> headers = { tr("Date"), tr("Time"), tr("Delay") };
        return headers[section];
    }
    void setRows(std::vector<UndergroundTimeRow> values)
    {
        beginResetModel();
        rows = std::move(values);
        endResetModel();
    }
private:
    std::vector<UndergroundTimeRow> rows;
};

DirtField4::DirtField4(QWidget *parent) : QWidget(parent)
{
    setFixedSize(416, 320);
    reset();
}

const std::array<u8, 130> &DirtField4::getDirt() const { return dirt; }

void DirtField4::reset()
{
    dirt.fill(2);
    painted.fill(false);
    update();
}

int DirtField4::tileAt(const QPoint &position) const
{
    constexpr int cell = 32;
    const int x = position.x() / cell;
    const int y = position.y() / cell;
    return x >= 0 && x < 13 && y >= 0 && y < 10 ? y * 13 + x : -1;
}

void DirtField4::paintTile(int index)
{
    if (index < 0 || painted[index]) return;
    dirt[index] = paintDepth;
    painted[index] = true;
    update(QRect((index % 13) * 32, (index / 13) * 32, 32, 32));
}

void DirtField4::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) return;
    const int index = tileAt(event->position().toPoint());
    if (index < 0) return;
    painted.fill(false);
    paintDepth = dirt[index] == 2 ? 4 : dirt[index] == 4 ? 6 : 2;
    painting = true;
    paintTile(index);
}

void DirtField4::mouseMoveEvent(QMouseEvent *event)
{
    if (painting && (event->buttons() & Qt::LeftButton)) paintTile(tileAt(event->position().toPoint()));
}

void DirtField4::mouseReleaseEvent(QMouseEvent *) { painting = false; }

void DirtField4::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    static const QPixmap atlas(QStringLiteral(":/images/underground_sprite_map.png"));
    for (int index = 0; index < static_cast<int>(dirt.size()); index++)
    {
        const int atlasX = dirt[index] == 6 ? 3 : dirt[index] == 4 ? 2 : 1;
        painter.drawPixmap((index % 13) * 32, (index / 13) * 32, atlas.copy(atlasX * 32, 0, 32, 32));
    }
}

MiningField4::MiningField4(QWidget *parent) : QWidget(parent)
{
    setFixedSize(416, 320);
}

void MiningField4::setResult(const UndergroundResult &value)
{
    result = value;
    update();
}

void MiningField4::setDisplayMode(u8 mode)
{
    displayMode = mode;
    update();
}

QSize MiningField4::minimumSizeHint() const
{
    return { 416, 320 };
}

void MiningField4::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    constexpr int cell = 32;
    const int fieldWidth = cell * 13;
    const int fieldHeight = cell * 10;
    const int left = (width() - fieldWidth) / 2;
    const int top = (height() - fieldHeight) / 2;
    static const QPixmap atlas(QStringLiteral(":/images/underground_sprite_map.png"));
    auto tile = [&](int atlasX) { return atlas.copy(atlasX * cell, 0, cell, cell); };
    for (int y = 0; y < 10; y++) for (int x = 0; x < 13; x++)
    {
        const int depth = result.dirt[y * 13 + x];
        const int atlasX = displayMode != 0 ? (depth >= 6 ? 3 : depth >= 4 ? 2 : depth >= 2 ? 1 : 0) : 0;
        painter.drawPixmap(left + x * cell, top + y * cell, tile(atlasX));
    }
    if (displayMode == 2 || atlas.isNull()) return;

    if (displayMode == 1)
    {
        for (const auto &object : result.objects)
        {
            for (u8 y = 0; y < object.height; y++) for (u8 x = 0; x < object.width; x++)
            {
                if (object.shape & (1ULL << (y * object.width + x)))
                    painter.drawPixmap(left + (object.x + x) * cell, top + (object.y + y) * cell, tile(0));
            }
        }
    }
    for (const auto &object : result.objects)
    {
        if (!object.metal)
        {
            const auto &sprite = undergroundSprites[object.item];
            QPixmap image = atlas.copy(sprite.x * cell, sprite.y * cell, sprite.width * cell, sprite.height * cell);
            const int rotation = spriteRotation(sprite, object);
            if (rotation) image = image.transformed(QTransform().rotate(rotation), Qt::FastTransformation);
            painter.drawPixmap(left + object.x * cell, top + object.y * cell, image);
            continue;
        }

        int occupied = std::popcount(object.shape);
        for (u8 y = 0; y < object.height; y++) for (u8 x = 0; x < object.width; x++)
        {
            if (!(object.shape & (1ULL << (y * object.width + x)))) continue;
            auto has = [&](int checkX, int checkY) {
                return checkX >= 0 && checkX < object.width && checkY >= 0 && checkY < object.height
                    && (object.shape & (1ULL << (checkY * object.width + checkX)));
            };
            u8 directions = 0;
            if (has(x, y - 1)) directions |= 1;
            if (has(x + 1, y)) directions |= 2;
            if (has(x, y + 1)) directions |= 4;
            if (has(x - 1, y)) directions |= 8;
            const int connections = std::popcount(directions);
            int metalSprite;
            u8 baseDirections;
            if (connections == 4) { metalSprite = 1; baseDirections = 15; }
            else if (connections == 3 && occupied == 4) { metalSprite = 3; baseDirections = 11; }
            else if (connections == 3) { metalSprite = 2; baseDirections = 14; }
            else if (connections == 2 && (directions == 5 || directions == 10)) { metalSprite = 4; baseDirections = 10; }
            else if (connections == 2)
            {
                const int diagonalX = x + ((directions & 2) ? 1 : -1);
                const int diagonalY = y + ((directions & 4) ? 1 : -1);
                metalSprite = has(diagonalX, diagonalY) ? 5 : 6;
                baseDirections = metalSprite == 5 ? 6 : 3;
            }
            else { metalSprite = 7; baseDirections = 4; }
            int rotation = 0;
            u8 rotated = baseDirections;
            while (rotated != directions && rotation < 270)
            {
                rotated = static_cast<u8>(((rotated << 1) & 0xF) | ((rotated >> 3) & 1));
                rotation += 90;
            }
            QPixmap image = tile(3 + metalSprite);
            if (rotation) image = image.transformed(QTransform().rotate(rotation), Qt::FastTransformation);
            painter.drawPixmap(left + (object.x + x) * cell, top + (object.y + y) * cell, image);
        }
    }
}

Underground4::Underground4(QWidget *parent) : QWidget(parent), currentProfile(nullptr), currentItemCount(0), advanceCacheValid(false)
{
    setWindowTitle(tr("Underground"));
    setAttribute(Qt::WA_QuitOnClose, false);
    setAttribute(Qt::WA_DeleteOnClose);
    auto *root = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    auto *generatorPage = new QWidget(tabs);
    generatorPage->setAutoFillBackground(true);
    generatorPage->setBackgroundRole(QPalette::Window);
    auto *generatorLayout = new QVBoxLayout(generatorPage);
    generatorLayout->setContentsMargins(4, 4, 4, 4);
    auto *finderPage = new QWidget(tabs);
    finderPage->setAutoFillBackground(true);
    finderPage->setBackgroundRole(QPalette::Window);
    auto *searcherPage = new QWidget(tabs);
    searcherPage->setAutoFillBackground(true);
    searcherPage->setBackgroundRole(QPalette::Window);
    tabs->addTab(generatorPage, tr("Generator"));
    tabs->addTab(finderPage, tr("Finder"));
    tabs->addTab(searcherPage, tr("Searcher"));
    root->addWidget(tabs);
    profileDisplay = new ProfileDisplay4(this);
    profileDisplay->setup(settingPrefix, Game::DPPt, Game::Platinum, true);
    profileDisplay->setTitle(QString());
    minedPlates = new ComboMenu(profileDisplay);
    minedPlates->setMultiSelect(true);
    for (int i = 0; i < 16; i++) minedPlates->addAction(tr(plateNames[i]), i);
    minedPlates->setCheckedData({});
    minedPlates->setText(tr("None"));
    profileDisplay->addControl(tr("Plates"), minedPlates);
    root->insertWidget(0, profileDisplay);
    QTimer::singleShot(0, this, [this] {
        minedPlates->setFixedWidth(minedPlates->width());
        updatePlateText();
    });

    auto *rngInfo = new QGroupBox(tr("RNG Info"), this);
    auto *settings = new QGridLayout(rngInfo);
    seed = new TextBox(rngInfo);
    seed->setValues(InputType::Seed64Bit);
    generateButton = new QPushButton(tr("Generate"), rngInfo);
    auto *backButton = new QPushButton(tr("Back"), rngInfo);
    auto *resetButton = new QPushButton(tr("Reset"), rngInfo);
    backButton->setEnabled(false);
    resetButton->setEnabled(false);
    settings->addWidget(new QLabel(tr("Seed"), rngInfo), 0, 0);
    settings->addWidget(seed, 0, 1);
    settings->addWidget(generateButton, 0, 2);
    settings->addWidget(backButton, 1, 1);
    settings->addWidget(resetButton, 1, 2);
    settings->setColumnStretch(1, 1);
    rngInfo->setMaximumWidth(300);

    auto makeSpot = [this](const QString &title, MiningField4 *&field, QLabel *&itemNames, QSpinBox *&frame) {
        auto *group = new QGroupBox(title, this);
        auto *layout = new QVBoxLayout(group);
        auto *tabs = new QTabBar(group);
        tabs->setExpanding(false);
        tabs->addTab(tr("Items"));
        tabs->addTab(tr("Both"));
        tabs->addTab(tr("Wall"));
        tabs->setCurrentIndex(1);
        itemNames = new QLabel(group);
        field = new MiningField4(group);
        frame = new QSpinBox(group);
        frame->setRange(0, std::numeric_limits<int>::max());
        frame->hide();
        MiningField4 *fieldValue = field;
        connect(tabs, &QTabBar::currentChanged, fieldValue, [fieldValue](int index) { fieldValue->setDisplayMode(index); });
        layout->addWidget(tabs, 0, Qt::AlignLeft);
        layout->addWidget(field, 0, Qt::AlignLeft);
        layout->addWidget(itemNames);
        return group;
    };

    auto *currentSpot = makeSpot(tr("Current Mining Spot"), currentField, currentItemNames, currentFrame);
    auto *nextSpot = makeSpot(tr("Next Mining Spot"), nextField, nextItemNames, nextFrame);

    auto *middle = new QVBoxLayout;
    auto *settingsGroup = new QGroupBox(tr("Settings"), this);
    auto *controls = new QFormLayout(settingsGroup);
    outcomePriority = new QComboBox(settingsGroup);
    outcomePriority->addItems({ tr("Wall Collapsed"), tr("All Item Recovered") });
    outcomePriority->setCurrentIndex(1);
    controls->addRow(tr("Outcome Priority"), outcomePriority);
    auto *requiredWidget = new QWidget(settingsGroup);
    auto *requiredLayout = new QHBoxLayout(requiredWidget);
    requiredLayout->setContentsMargins(0, 0, 0, 0);
    for (int i = 0; i < 4; i++)
    {
        requiredItems[i] = new QCheckBox(QString::number(i + 1), requiredWidget);
        requiredItems[i]->setEnabled(false);
        requiredLayout->addWidget(requiredItems[i]);
    }
    controls->addRow(tr("Required Items"), requiredWidget);
    onlyTargetAdvances = new QCheckBox(tr("Only Display Target Advances"), settingsGroup);
    controls->addRow(onlyTargetAdvances);
    middle->addWidget(settingsGroup, 1);

    auto *filtersGroup = new QGroupBox(tr("Filters"), this);
    auto *filters = new QGridLayout(filtersGroup);
    for (int i = 0; i < 4; i++)
    {
        auto *itemFilter = new ComboBox(filtersGroup);
        itemFilter->enableAutoComplete();
        itemFilters[i] = itemFilter;
        itemFilters[i]->addItem(tr("Any"), 0);
        itemFilterCounts[i] = new QSpinBox(filtersGroup);
        itemFilterCounts[i]->setRange(1, 4);
        itemFilterCounts[i]->setFixedWidth(55);
        itemFilterCounts[i]->setEnabled(false);
        filters->addWidget(itemFilters[i], i, 0);
        filters->addWidget(itemFilterCounts[i], i, 1);
        connect(itemFilters[i], &QComboBox::currentIndexChanged, itemFilterCounts[i], [this, i](int index) {
            Q_UNUSED(index);
            updateUndergroundFilterCount(itemFilters[i], itemFilterCounts[i]);
            applyItemFilters();
        });
        connect(itemFilterCounts[i], &QSpinBox::valueChanged, this, [this] { applyItemFilters(); });
    }
    auto *resetFilters = new QPushButton(tr("Reset"), filtersGroup);
    filters->addWidget(resetFilters, 4, 0, 1, 2);
    connect(resetFilters, &QPushButton::clicked, this, [this] {
        for (size_t i = 0; i < itemFilters.size(); i++)
        {
            itemFilters[i]->blockSignals(true);
            itemFilterCounts[i]->blockSignals(true);
            itemFilters[i]->setCurrentIndex(0);
            itemFilterCounts[i]->setValue(1);
            itemFilters[i]->blockSignals(false);
            itemFilterCounts[i]->blockSignals(false);
            updateUndergroundFilterCount(itemFilters[i], itemFilterCounts[i]);
        }
        applyItemFilters();
    });
    filters->setColumnStretch(0, 1);
    middle->addWidget(filtersGroup, 1);
    middle->addWidget(rngInfo);
    auto *top = new QHBoxLayout;
    top->addWidget(currentSpot);
    top->addLayout(middle, 1);
    top->addWidget(nextSpot);
    generatorLayout->addLayout(top);

    advances = new TableView(this);
    advanceModel = new UndergroundAdvanceModel(advances);
    advances->setModel(advanceModel);
    advances->setSelectionBehavior(QAbstractItemView::SelectRows);
    advances->setSelectionMode(QAbstractItemView::SingleSelection);
    advances->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    advances->horizontalHeader()->setStretchLastSection(false);
    auto *setAsCurrent = advances->addAction(tr("Set as Current Mining Spot"));
    generatorLayout->addWidget(advances, 1);

    auto *finderLayout = new QGridLayout(finderPage);
    finderLayout->setContentsMargins(4, 4, 4, 4);
    auto *fieldGroup = new QGroupBox(tr("Your Current Mining Spot"), finderPage);
    auto *fieldLayout = new QVBoxLayout(fieldGroup);
    finderField = new DirtField4(fieldGroup);
    auto *resetField = new QPushButton(tr("Reset"), fieldGroup);
    fieldLayout->addWidget(finderField, 0, Qt::AlignLeft);
    fieldLayout->addWidget(resetField);
    finderLayout->addWidget(fieldGroup, 0, 0);

    auto *previewGroup = new QGroupBox(tr("Selected Seeds Mining Spot"), finderPage);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    auto *previewTabs = new QTabBar(previewGroup);
    previewTabs->setExpanding(false);
    previewTabs->addTab(tr("Items"));
    previewTabs->addTab(tr("Both"));
    previewTabs->addTab(tr("Wall"));
    previewTabs->setCurrentIndex(2);
    finderPreview = new MiningField4(previewGroup);
    chainerPreview = finderPreview;
    chainerItemNames = new QLabel(previewGroup);
    finderPreview->setDisplayMode(2);
    connect(previewTabs, &QTabBar::currentChanged, finderPreview,
            [this](int index) { finderPreview->setDisplayMode(index); });
    UndergroundResult previewResult;
    previewResult.dirt.fill(2);
    finderPreview->setResult(previewResult);
    previewLayout->addWidget(previewTabs, 0, Qt::AlignLeft);
    previewLayout->addWidget(finderPreview, 0, Qt::AlignLeft | Qt::AlignTop);
    previewLayout->addWidget(chainerItemNames);
    previewLayout->addStretch();

    auto *finderRngGroup = new QGroupBox(tr("RNG Info"), finderPage);
    auto *finderRng = new QFormLayout(finderRngGroup);
    finderDate = new DateEdit(finderRngGroup);
    finderDate->setDate(QDate::currentDate());
    finderTime = new QTimeEdit(finderRngGroup);
    finderTime->setDisplayFormat(QStringLiteral("HH:mm:ss"));
    finderTime->setTime(QTime::currentTime());
    finderTargetDelay = new TextBox(finderRngGroup);
    finderTargetDelay->setValues(InputType::Delay);
    finderTargetDelay->setText(QStringLiteral("0"));
    finderDelayRange = new TextBox(finderRngGroup);
    finderDelayRange->setValues(InputType::Advance32Bit);
    finderDelayRange->setText(QStringLiteral("500"));
    finderItemCount = new QComboBox(finderRngGroup);
    finderItemCount->addItem(tr("Any"), 0);
    finderItemCount->addItem(QStringLiteral("2"), 2);
    finderItemCount->addItem(QStringLiteral("3"), 3);
    finderItemCount->addItem(QStringLiteral("4"), 4);
    finderMaxTileMismatches = new QSpinBox(finderRngGroup);
    finderMaxTileMismatches->setRange(0, 130);
    finderMaxTileMismatches->setValue(30);
    auto *findButton = new QPushButton(tr("Search"), finderRngGroup);
    finderRng->addRow(tr("Date"), finderDate);
    finderRng->addRow(tr("Time"), finderTime);
    finderRng->addRow(tr("Target Delay"), finderTargetDelay);
    finderRng->addRow(tr("Delay +/-"), finderDelayRange);
    finderRng->addRow(tr("Item Count"), finderItemCount);
    finderRng->addRow(tr("Max Tile Mismatches"), finderMaxTileMismatches);
    finderRng->addRow(findButton);
    auto *finderControls = new QVBoxLayout;
    finderControls->addWidget(finderRngGroup);

    {
    auto *chainerOptionsGroup = new QGroupBox(tr("Item Finder"), finderPage);
    auto *chainerOptions = new QFormLayout(chainerOptionsGroup);
    chainerSeed = new TextBox(chainerOptionsGroup);
    chainerSeed->setValues(InputType::Seed64Bit);
    chainerItem = new ComboBox(chainerOptionsGroup);
    chainerItem->enableAutoComplete();
    chainerMaxSpots = new QSpinBox(chainerOptionsGroup);
    chainerMaxSpots->setRange(1, 8);
    chainerMaxSpots->setValue(1);
    chainerRecoverAllPlates = new QCheckBox(chainerOptionsGroup);
    auto *plateOption = new QWidget(chainerOptionsGroup);
    auto *plateOptionLayout = new QHBoxLayout(plateOption);
    plateOptionLayout->setContentsMargins(0, 0, 0, 0);
    auto *plateOptionText = new QLabel(tr("Always Recover All Plates"), plateOption);
    plateOptionText->setWordWrap(true);
    plateOptionLayout->addWidget(chainerRecoverAllPlates, 0, Qt::AlignTop);
    plateOptionLayout->addWidget(plateOptionText, 1);
    chainerSearchButton = new QPushButton(tr("Search"), chainerOptionsGroup);
    chainerCancelButton = new QPushButton(tr("Cancel"), chainerOptionsGroup);
    chainerCancelButton->setEnabled(false);
    chainerProgress = new QProgressBar(chainerOptionsGroup);
    chainerProgress->setRange(0, 100);
    chainerProgress->setValue(0);
    chainerRecovered = new QLabel(QStringLiteral("0"), chainerOptionsGroup);
    chainerOptions->addRow(tr("Seed"), chainerSeed);
    chainerOptions->addRow(tr("Target Item"), chainerItem);
    chainerOptions->addRow(tr("Max Mining Spots"), chainerMaxSpots);
    chainerOptions->addRow(plateOption);
    auto *chainerButtons = new QWidget(chainerOptionsGroup);
    auto *chainerButtonsLayout = new QHBoxLayout(chainerButtons);
    chainerButtonsLayout->setContentsMargins(0, 0, 0, 0);
    chainerButtonsLayout->addWidget(chainerSearchButton);
    chainerButtonsLayout->addWidget(chainerCancelButton);
    chainerOptions->addRow(chainerButtons);
    chainerOptions->addRow(chainerProgress);
    chainerOptions->addRow(tr("Maximum Recovered"), chainerRecovered);
    finderControls->addWidget(chainerOptionsGroup);
    }
    finderControls->addStretch();
    finderLayout->addLayout(finderControls, 0, 1);
    finderLayout->addWidget(previewGroup, 0, 2);
    auto *finderTable = new TableView(finderPage);
    finderModel = new UndergroundFinderModel(finderTable);
    auto *finderProxy = new SortFilterProxyModel(finderTable, finderModel);
    finderTable->setModel(finderProxy);
    finderTable->setSortingEnabled(true);
    finderTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    finderTable->horizontalHeader()->setStretchLastSection(false);
    auto *goToGenerator = finderTable->addAction(tr("Go to Generator"));
    finderTable->setPrimaryAction(goToGenerator);
    chainerTable = new TableView(finderPage);
    chainerModel = new UndergroundChainerModel(chainerTable);
    chainerTable->setModel(chainerModel);
    chainerTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    chainerTable->setSelectionMode(QAbstractItemView::SingleSelection);
    chainerTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    chainerTable->horizontalHeader()->setStretchLastSection(false);
    auto *setChainerStart = chainerTable->addAction(tr("Set as Starting Seed"));
    auto *chainerGoToGenerator = chainerTable->addAction(tr("Go to Generator"));
    chainerTable->setPrimaryAction(setChainerStart);
    auto *finderBottom = new QHBoxLayout;
    finderBottom->setContentsMargins(0, 0, 0, 0);
    finderBottom->addWidget(finderTable, 35);
    finderBottom->addWidget(chainerTable, 65);
    finderLayout->addLayout(finderBottom, 1, 0, 1, 3);
    finderLayout->setColumnStretch(1, 1);
    finderLayout->setRowStretch(1, 1);

    auto *searcherLayout = new QHBoxLayout(searcherPage);
    searcherLayout->setContentsMargins(4, 4, 4, 4);
    auto *searcherLeft = new QVBoxLayout;
    auto *searcherRngGroup = new QGroupBox(tr("RNG Info"), searcherPage);
    auto *searcherRng = new QFormLayout(searcherRngGroup);
    searcherStartDate = new DateEdit(searcherRngGroup);
    searcherStartDate->setDate(QDate::currentDate());
    searcherEndDate = new DateEdit(searcherRngGroup);
    searcherEndDate->setDate(QDate::currentDate());
    searcherMinDelay = new TextBox(searcherRngGroup);
    searcherMinDelay->setValues(InputType::Delay);
    searcherMinDelay->setText(QStringLiteral("1400"));
    searcherMaxDelay = new TextBox(searcherRngGroup);
    searcherMaxDelay->setValues(InputType::Delay);
    searcherMaxDelay->setText(QStringLiteral("2000"));
    searcherMaxWalls = new QSpinBox(searcherRngGroup);
    searcherMaxWalls->setRange(1, 8);
    searcherMaxWalls->setValue(1);
    searcherSearchButton = new QPushButton(tr("Search"), searcherRngGroup);
    searcherCancelButton = new QPushButton(tr("Cancel"), searcherRngGroup);
    searcherCancelButton->setEnabled(false);
    searcherProgress = new QProgressBar(searcherRngGroup);
    searcherProgress->setValue(0);
    searcherRng->addRow(tr("Start Date"), searcherStartDate);
    searcherRng->addRow(tr("End Date"), searcherEndDate);
    searcherRng->addRow(tr("Min Delay"), searcherMinDelay);
    searcherRng->addRow(tr("Max Delay"), searcherMaxDelay);
    searcherRng->addRow(tr("Max Mining Spots"), searcherMaxWalls);
    auto *searchButtons = new QWidget(searcherRngGroup);
    auto *searchButtonsLayout = new QHBoxLayout(searchButtons);
    searchButtonsLayout->setContentsMargins(0, 0, 0, 0);
    searchButtonsLayout->addWidget(searcherSearchButton);
    searchButtonsLayout->addWidget(searcherCancelButton);
    searcherRng->addRow(searchButtons);
    searcherRng->addRow(searcherProgress);
    searcherLeft->addWidget(searcherRngGroup);

    auto *searcherFiltersGroup = new QGroupBox(tr("Filters"), searcherPage);
    auto *searcherFiltersLayout = new QGridLayout(searcherFiltersGroup);
    searcherRecoverAllPlates = new QCheckBox(
        tr("Consider a Plate Recovered for subsequent spots, even if that Plate is not the target"), searcherFiltersGroup);
    searcherFiltersLayout->addWidget(searcherRecoverAllPlates, 0, 0, 1, 2);
    for (int i = 0; i < 4; i++)
    {
        auto *itemFilter = new ComboBox(searcherFiltersGroup);
        itemFilter->enableAutoComplete();
        searcherItemFilters[i] = itemFilter;
        itemFilter->addItem(tr("Any"), 0);
        searcherItemFilterCounts[i] = new QSpinBox(searcherFiltersGroup);
        searcherItemFilterCounts[i]->setRange(1, 20);
        searcherItemFilterCounts[i]->setFixedWidth(55);
        searcherItemFilterCounts[i]->setEnabled(false);
        searcherFiltersLayout->addWidget(itemFilter, i + 1, 0);
        searcherFiltersLayout->addWidget(searcherItemFilterCounts[i], i + 1, 1);
        connect(itemFilter, &QComboBox::currentIndexChanged, searcherItemFilterCounts[i], [this, i](int index) {
            Q_UNUSED(index);
            updateUndergroundFilterCount(searcherItemFilters[i], searcherItemFilterCounts[i], 4 * searcherMaxWalls->value());
        });
    }
    connect(searcherMaxWalls, &QSpinBox::valueChanged, this, [this] {
        for (size_t i = 0; i < searcherItemFilters.size(); i++)
            updateUndergroundFilterCount(searcherItemFilters[i], searcherItemFilterCounts[i], 4 * searcherMaxWalls->value());
    });
    auto *resetSearcherFilters = new QPushButton(tr("Reset"), searcherFiltersGroup);
    searcherFiltersLayout->addWidget(resetSearcherFilters, 5, 0, 1, 2);
    connect(resetSearcherFilters, &QPushButton::clicked, this, [this] {
        searcherRecoverAllPlates->setChecked(false);
        for (size_t i = 0; i < searcherItemFilters.size(); i++)
        {
            searcherItemFilters[i]->setCurrentIndex(0);
            searcherItemFilterCounts[i]->setValue(1);
            updateUndergroundFilterCount(searcherItemFilters[i], searcherItemFilterCounts[i], 4 * searcherMaxWalls->value());
        }
    });
    searcherFiltersLayout->setColumnStretch(0, 1);
    searcherLeft->addWidget(searcherFiltersGroup);

    searcherTable = new TableView(searcherPage);
    searcherModel = new UndergroundSearcherModel(searcherTable);
    auto *searcherProxy = new SortFilterProxyModel(searcherTable, searcherModel);
    searcherTable->setModel(searcherProxy);
    searcherTable->setSortingEnabled(true);
    searcherTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    searcherTable->setSelectionMode(QAbstractItemView::SingleSelection);
    searcherTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    searcherTable->horizontalHeader()->setStretchLastSection(false);
    auto *searcherGoToGenerator = searcherTable->addAction(tr("Go to Generator"));
    searcherTable->setPrimaryAction(searcherGoToGenerator);
    searcherLeft->addWidget(searcherTable, 1);
    searcherLayout->addLayout(searcherLeft, 1);

    auto *searcherRight = new QVBoxLayout;
    auto *searcherPreviewGroup = new QGroupBox(tr("Selected Seeds Mining Spot"), searcherPage);
    auto *searcherPreviewLayout = new QVBoxLayout(searcherPreviewGroup);
    auto *searcherPreviewTabs = new CompactTabBar(searcherPreviewGroup);
    searcherPreviewTabs->setExpanding(false);
    searcherPreviewTabs->setUsesScrollButtons(false);
    auto *compactTabTextStyle = new CompactTabTextStyle(searcherPreviewTabs->style()->objectName());
    compactTabTextStyle->setParent(searcherPreviewTabs);
    searcherPreviewTabs->setStyle(compactTabTextStyle);
    auto *searcherPreviewStack = new QStackedLayout;
    for (size_t wall = 0; wall < searcherPreviews.size(); wall++)
    {
        searcherPreviews[wall] = new MiningField4(searcherPreviewGroup);
        searcherPreviewStack->addWidget(searcherPreviews[wall]);
        searcherPreviewStack->setAlignment(searcherPreviews[wall], Qt::AlignLeft | Qt::AlignTop);
        searcherPreviewTabs->addTab(tr("Spot %1").arg(wall + 1));
    }
    connect(searcherPreviewTabs, &QTabBar::currentChanged, searcherPreviewStack, &QStackedLayout::setCurrentIndex);
    searcherPreviewLayout->addWidget(searcherPreviewTabs, 0, Qt::AlignLeft);
    searcherPreviewLayout->addLayout(searcherPreviewStack);
    searcherRight->addWidget(searcherPreviewGroup);

    auto *timesGroup = new QGroupBox(tr("Seed to Time"), searcherPage);
    auto *timesLayout = new QVBoxLayout(timesGroup);
    auto *timesInput = new QFormLayout;
    searcherSeed = new TextBox(timesGroup);
    searcherSeed->setValues(InputType::Seed64Bit);
    auto *generateTimesButton = new QPushButton(tr("Generate Times"), timesGroup);
    timesInput->addRow(tr("Seed"), searcherSeed);
    timesInput->addRow(generateTimesButton);
    timesLayout->addLayout(timesInput);
    auto *timesTable = new TableView(timesGroup);
    searcherTimeModel = new UndergroundTimeModel(timesTable);
    auto *timesProxy = new SortFilterProxyModel(timesTable, searcherTimeModel);
    timesTable->setModel(timesProxy);
    timesTable->setSortingEnabled(true);
    timesTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    timesTable->horizontalHeader()->setStretchLastSection(true);
    auto *goToFinder = timesTable->addAction(tr("Go to Finder"));
    timesTable->setPrimaryAction(goToFinder);
    timesLayout->addWidget(timesTable, 1);
    searcherRight->addWidget(timesGroup, 1);
    searcherLayout->addLayout(searcherRight);

    connect(profileDisplay, &ProfileDisplay4::profileChanged, this, &Underground4::profileChanged);
    connect(profileDisplay, &ProfileDisplay4::profilesChanged, this, &Underground4::profilesChanged);
    connect(generateButton, &QPushButton::clicked, this, &Underground4::generate);
    connect(seed, &QLineEdit::textChanged, this, [this, backButton, resetButton] {
        advanceCacheValid = false;
        currentFrame->setValue(0);
        miningFieldHistory.clear();
        miningPlateHistory.clear();
        targetRoute.clear();
        targetPlateRoute.clear();
        advances->clearTargetAdvance();
        backButton->setEnabled(false);
        resetButton->setEnabled(false);
    });
    connect(backButton, &QPushButton::clicked, this, [this, backButton, resetButton] {
        if (miningFieldHistory.empty()) return;
        targetRoute.clear();
        targetPlateRoute.clear();
        currentFrame->setValue(static_cast<int>(miningFieldHistory.back()));
        miningFieldHistory.pop_back();
        if (!miningPlateHistory.empty())
        {
            generatorTemporaryPlates = miningPlateHistory.back();
            miningPlateHistory.pop_back();
        }
        advanceCacheValid = false;
        generate();
        backButton->setEnabled(!miningFieldHistory.empty());
        resetButton->setEnabled(currentFrame->value() != 0);
    });
    connect(resetButton, &QPushButton::clicked, this, [this, backButton, resetButton] {
        miningFieldHistory.clear();
        miningPlateHistory.clear();
        targetRoute.clear();
        targetPlateRoute.clear();
        currentFrame->setValue(0);
        generatorTemporaryPlates = generatorInitialTemporaryPlates;
        advanceCacheValid = false;
        generate();
        backButton->setEnabled(false);
        resetButton->setEnabled(false);
    });
    connect(outcomePriority, &QComboBox::currentIndexChanged, this, [this] { advanceCacheValid = false; });
    for (QCheckBox *required : requiredItems)
        connect(required, &QCheckBox::toggled, this, [this] { advanceCacheValid = false; });
    connect(onlyTargetAdvances, &QCheckBox::toggled, this, &Underground4::applyItemFilters);
    connect(resetField, &QPushButton::clicked, finderField, &DirtField4::reset);
    connect(findButton, &QPushButton::clicked, this, &Underground4::find);
    connect(searcherSearchButton, &QPushButton::clicked, this, &Underground4::search);
    connect(chainerSearchButton, &QPushButton::clicked, this, &Underground4::chain);
    connect(generateTimesButton, &QPushButton::clicked, this, &Underground4::generateTimes);
    connect(goToFinder, &QAction::triggered, this, [this, tabs, timesTable] {
        const QModelIndex index = timesTable->currentIndex();
        if (!index.isValid()) return;
        const QDate date = QDate::fromString(index.siblingAtColumn(0).data().toString(), QStringLiteral("yyyy-MM-dd"));
        const QTime time = QTime::fromString(index.siblingAtColumn(1).data().toString(), QStringLiteral("HH:mm:ss"));
        if (!date.isValid() || !time.isValid()) return;
        finderDate->setDate(date);
        finderTime->setTime(time);
        finderTargetDelay->setText(index.siblingAtColumn(2).data().toString());
        tabs->setCurrentIndex(1);
    });
    connect(goToGenerator, &QAction::triggered, this, [this, tabs, finderTable, backButton, resetButton] {
        const QModelIndex index = finderTable->currentIndex();
        if (!index.isValid()) return;
        seed->setText(index.siblingAtColumn(0).data().toString());
        generatorTemporaryPlates = 0;
        generatorInitialTemporaryPlates = 0;
        targetPlateRoute.clear();
        currentFrame->setValue(0);
        miningFieldHistory.clear();
        miningPlateHistory.clear();
        backButton->setEnabled(false);
        resetButton->setEnabled(false);
        tabs->setCurrentIndex(0);
        generate();
    });
    connect(searcherGoToGenerator, &QAction::triggered, this, [this, tabs, backButton, resetButton, searcherProxy] {
        const QModelIndex index = searcherTable->currentIndex();
        if (!index.isValid()) return;
        const QModelIndex sourceIndex = searcherProxy->mapToSource(index);
        if (!sourceIndex.isValid()) return;
        const UndergroundSearcherRow &row = searcherModel->getRow(sourceIndex.row());
        seed->setText(index.siblingAtColumn(0).data().toString());
        generatorTemporaryPlates = 0;
        generatorInitialTemporaryPlates = 0;
        currentFrame->setValue(0);
        miningFieldHistory.clear();
        miningPlateHistory.clear();
        targetRoute = row.routeAdvances;
        targetPlateRoute = row.routePlateMasks;
        backButton->setEnabled(false);
        resetButton->setEnabled(false);
        tabs->setCurrentIndex(0);
        generate();
    });
    connect(finderTable->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
        if (!currentProfile || !current.isValid()) return;
        bool valid = false;
        const u64 selectedSeed = current.siblingAtColumn(0).data().toString().toULongLong(&valid, 16);
        if (valid)
        {
            const QString value = current.siblingAtColumn(0).data().toString();
            chainerTemporaryPlates = 0;
            chainerSeed->setText(value);
            const UndergroundResult result = getGenerator().generate(selectedSeed);
            finderPreview->setResult(result);
            updateItemNames(chainerItemNames, result);
        }
    });
    connect(searcherTable->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this, searcherProxy](const QModelIndex &current, const QModelIndex &) {
        if (!currentProfile || !current.isValid()) return;
        const QModelIndex sourceIndex = searcherProxy->mapToSource(current);
        if (!sourceIndex.isValid()) return;
        const UndergroundSearcherRow &row = searcherModel->getRow(sourceIndex.row());
        const QString value = current.siblingAtColumn(0).data().toString();
        bool valid = false;
        const u64 selectedSeed = value.toULongLong(&valid, 16);
        if (!valid) return;
        searcherSeed->setText(value);
        UndergroundGenerator previewGenerator = getGenerator();
        searcherPreviews[0]->setResult(previewGenerator.generate(selectedSeed));
        for (size_t wall = 1; wall < searcherPreviews.size(); wall++)
        {
            if (wall < row.walls && wall - 1 < row.routeAdvances.size() && wall - 1 < row.routePlateMasks.size())
            {
                UndergroundGenerator wallGenerator = getGenerator();
                const u16 plateMask = row.routePlateMasks[wall - 1];
                for (u8 plate = 0; plate < 16; plate++)
                    if (plateMask & (1 << plate)) wallGenerator.setMinedPlate(44 + plate);
                searcherPreviews[wall]->setResult(
                    wallGenerator.generate(UndergroundGenerator::advanceSeed(selectedSeed, row.routeAdvances[wall - 1])));
            }
            else
            {
                searcherPreviews[wall]->setResult({});
            }
        }
        generateTimes();
    });
    connect(chainerTable->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;
        const UndergroundChainerRow &row = chainerModel->getRow(current.row());
        chainerPreview->setResult(row.result);
        updateItemNames(chainerItemNames, row.result);
    });
    connect(setChainerStart, &QAction::triggered, this, [this] {
        const QModelIndex index = chainerTable->currentIndex();
        if (!index.isValid()) return;
        const UndergroundChainerRow &row = chainerModel->getRow(index.row());
        chainerTemporaryPlates = row.platesBefore;
        chainerSeed->setText(QString::number(row.spotSeed, 16).toUpper());
    });
    connect(chainerGoToGenerator, &QAction::triggered, this, [this, tabs, backButton, resetButton] {
        const QModelIndex index = chainerTable->currentIndex();
        if (!index.isValid()) return;
        const int selected = index.row();
        const UndergroundChainerRow &row = chainerModel->getRow(selected);
        seed->setText(QString::number(row.spotSeed, 16).toUpper());
        generatorTemporaryPlates = row.platesBefore;
        generatorInitialTemporaryPlates = row.platesBefore;
        currentFrame->setValue(0);
        miningFieldHistory.clear();
        miningPlateHistory.clear();
        targetRoute.clear();
        targetPlateRoute.clear();
        for (int next = selected + 1; next < chainerModel->rowCount(); next++)
        {
            const UndergroundChainerRow &nextRow = chainerModel->getRow(next);
            targetRoute.push_back(nextRow.actualAdvance - row.actualAdvance);
            targetPlateRoute.push_back(nextRow.platesBefore);
        }
        advanceCacheValid = false;
        backButton->setEnabled(false);
        resetButton->setEnabled(false);
        tabs->setCurrentIndex(0);
        generate();
    });
    connect(chainerSeed, &QLineEdit::textEdited, this, [this] { chainerTemporaryPlates = 0; });
    connect(seed, &QLineEdit::textEdited, this, [this] {
        generatorTemporaryPlates = 0;
        generatorInitialTemporaryPlates = 0;
    });
    connect(minedPlates, &ComboMenu::checkedDataChanged, this, &Underground4::platesChanged);
    connect(advances->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex &current, const QModelIndex &) {
        if (!current.isValid()) return;
        nextFrame->setValue(static_cast<int>(advanceModel->getActualAdvance(current.row())));
        generateFrame(false);
    });
    connect(setAsCurrent, &QAction::triggered, this, [this, backButton, resetButton] {
        const QModelIndex index = advances->currentIndex();
        if (!index.isValid()) return;
        const u32 selectedAdvance = advanceModel->getActualAdvance(index.row());
        const u16 previousPlates = generatorTemporaryPlates;
        if (!targetRoute.empty() && selectedAdvance == targetRoute.front())
        {
            targetRoute.erase(targetRoute.begin());
            if (!targetPlateRoute.empty())
            {
                generatorTemporaryPlates = targetPlateRoute.front();
                targetPlateRoute.erase(targetPlateRoute.begin());
            }
        }
        else
        {
            targetRoute.clear();
            targetPlateRoute.clear();
        }
        miningFieldHistory.push_back(static_cast<u32>(currentFrame->value()));
        miningPlateHistory.push_back(previousPlates);
        currentFrame->setValue(static_cast<int>(selectedAdvance));
        advanceCacheValid = false;
        generate();
        backButton->setEnabled(true);
        resetButton->setEnabled(true);
    });
    updatePlateText();
    updateProfiles();
    minedPlates->setText(tr("None"));

    QSettings store;
    store.beginGroup(settingPrefix);
    store.remove(QStringLiteral("seed"));
    store.remove(QStringLiteral("finderItemCount"));
    store.remove(QStringLiteral("onlyTargetAdvances"));
    store.remove(QStringLiteral("chainerMaxSpots"));
    seed->setText(QStringLiteral("0"));
    finderTargetDelay->setText(store.value(QStringLiteral("finderTargetDelay"), finderTargetDelay->text()).toString());
    finderDelayRange->setText(store.value(QStringLiteral("finderDelayRange"), finderDelayRange->text()).toString());
    finderItemCount->setCurrentIndex(finderItemCount->findData(0));
    finderMaxTileMismatches->setValue(store.value(QStringLiteral("finderMaxTileMismatches"), 30).toInt());
    searcherStartDate->setDate(store.value(QStringLiteral("searcherStartDate"), QDate::currentDate()).toDate());
    searcherEndDate->setDate(store.value(QStringLiteral("searcherEndDate"), QDate::currentDate()).toDate());
    searcherMinDelay->setText(store.value(QStringLiteral("searcherMinDelay"), searcherMinDelay->text()).toString());
    searcherMaxDelay->setText(store.value(QStringLiteral("searcherMaxDelay"), searcherMaxDelay->text()).toString());
    searcherMaxWalls->setValue(store.value(QStringLiteral("searcherMaxWalls"), 1).toInt());
    searcherRecoverAllPlates->setChecked(store.value(QStringLiteral("searcherRecoverAllPlates"), false).toBool());
    chainerRecoverAllPlates->setChecked(store.value(QStringLiteral("chainerRecoverAllPlates"), false).toBool());
    if (store.contains(QStringLiteral("geometry"))) restoreGeometry(store.value(QStringLiteral("geometry")).toByteArray());
    else resize(1600, 900);
    store.endGroup();
}

Underground4::~Underground4()
{
    QSettings store;
    store.beginGroup(settingPrefix);
    store.setValue(QStringLiteral("finderTargetDelay"), finderTargetDelay->text());
    store.setValue(QStringLiteral("finderDelayRange"), finderDelayRange->text());
    store.setValue(QStringLiteral("finderMaxTileMismatches"), finderMaxTileMismatches->value());
    store.setValue(QStringLiteral("searcherStartDate"), searcherStartDate->date());
    store.setValue(QStringLiteral("searcherEndDate"), searcherEndDate->date());
    store.setValue(QStringLiteral("searcherMinDelay"), searcherMinDelay->text());
    store.setValue(QStringLiteral("searcherMaxDelay"), searcherMaxDelay->text());
    store.setValue(QStringLiteral("searcherMaxWalls"), searcherMaxWalls->value());
    store.setValue(QStringLiteral("searcherRecoverAllPlates"), searcherRecoverAllPlates->isChecked());
    store.setValue(QStringLiteral("chainerRecoverAllPlates"), chainerRecoverAllPlates->isChecked());
    store.setValue(QStringLiteral("geometry"), saveGeometry());
    store.endGroup();
}

void Underground4::updateProfiles() { profileDisplay->updateProfiles(); }
void Underground4::profileChanged(const Profile4 &profile)
{
    currentProfile = &profile;
    chainerTemporaryPlates = 0;
    generatorTemporaryPlates = 0;
    generatorInitialTemporaryPlates = 0;
    miningPlateHistory.clear();
    advanceCacheValid = false;
    targetRoute.clear();
    targetPlateRoute.clear();
    advances->clearTargetAdvance();
    std::vector<int> plates;
    const auto mined = profile.getMinedPlates();
    for (int i = 0; i < static_cast<int>(mined.size()); i++)
        if (mined[i]) plates.push_back(i);
    minedPlates->blockSignals(true);
    minedPlates->setCheckedData(plates);
    minedPlates->blockSignals(false);
    minedPlates->setEnabled(!ProfileLoader4::getProfiles(Game::DPPt).empty());
    updatePlateText();
    updateItemFilters();
    updateSearcherItemFilters();
    updateChainerItems();
}

void Underground4::platesChanged()
{
    chainerTemporaryPlates = 0;
    generatorTemporaryPlates = 0;
    generatorInitialTemporaryPlates = 0;
    miningPlateHistory.clear();
    updatePlateText();
    if (!currentProfile || !minedPlates->isEnabled()) return;

    const Profile4 original = *currentProfile;
    std::array<bool, 16> plates = {};
    for (int plate : minedPlates->getRawCheckedData()) plates[plate] = true;
    const Profile4 update(original.getName(), original.getVersion(), original.getTID(), original.getSID(), original.getNationalDex(),
                          original.getUnownDiscovered(), original.getUnownPuzzle(), plates, original.getUndergroundOrb());
    ProfileLoader4::updateProfile(update, original);
    profileDisplay->updateProfiles();
    profileDisplay->setProfile(update);
    emit profilesChanged(4);
}

void Underground4::find()
{
    std::vector<UndergroundFinderRow> rows;
    if (!currentProfile)
    {
        finderModel->setRows(std::move(rows));
        return;
    }

    const QDate date = finderDate->date();
    const QTime time = finderTime->time();
    const u32 targetDelay = finderTargetDelay->getUInt();
    const u32 delayRange = finderDelayRange->getUInt();
    const u32 minDelay = targetDelay > delayRange ? targetDelay - delayRange : 0;
    const u32 maxDelay = delayRange > std::numeric_limits<u32>::max() - targetDelay ? std::numeric_limits<u32>::max()
                                                                                   : targetDelay + delayRange;
    const auto &targetDirt = finderField->getDirt();
    const u8 itemCount = static_cast<u8>(finderItemCount->currentData().toInt());
    const int maxTileMismatches = finderMaxTileMismatches->value();
    const UndergroundGenerator generator = getGenerator();
    u64 baseSeed = static_cast<u64>(date.year() % 100);
    baseSeed = baseSeed * 16 + date.month();
    baseSeed = baseSeed * 32 + date.day();
    baseSeed = baseSeed * 32 + time.hour();
    baseSeed = baseSeed * 64 + time.minute();
    baseSeed = baseSeed * 64 + time.second();
    for (u32 delay = minDelay;; delay++)
    {
        const u64 candidateSeed = baseSeed + delay;
        const UndergroundResult result = generator.generate(candidateSeed);
        if (itemCount != 0 && result.itemCount != itemCount)
        {
            if (delay == maxDelay) break;
            continue;
        }
        int mismatches = 0;
        for (size_t tile = 0; tile < targetDirt.size() && mismatches <= maxTileMismatches; tile++)
            mismatches += result.dirt[tile] != targetDirt[tile];
        if (mismatches <= maxTileMismatches) rows.push_back({ candidateSeed, date, time, delay });
        if (delay == maxDelay) break;
    }
    finderModel->setRows(std::move(rows));
}

void Underground4::search()
{
    if (!currentProfile || searcherStartDate->date() > searcherEndDate->date()
        || searcherMinDelay->getUInt() > searcherMaxDelay->getUInt())
    {
        searcherProgress->setValue(0);
        searcherModel->setRows({});
        return;
    }

    const QDate startDate = searcherStartDate->date();
    const QDate endDate = searcherEndDate->date();
    const u32 minDelay = searcherMinDelay->getUInt();
    const u32 maxDelay = searcherMaxDelay->getUInt();
    const u8 maxWalls = static_cast<u8>(searcherMaxWalls->value());
    const UndergroundGenerator generator = getGenerator();
    std::array<std::pair<u8, u8>, 4> filters = {};
    for (size_t i = 0; i < filters.size(); i++)
        filters[i] = { static_cast<u8>(searcherItemFilters[i]->currentData().toInt()),
                       static_cast<u8>(searcherItemFilterCounts[i]->value()) };

    searcherModel->setRows({});
    searcherSearchButton->setEnabled(false);
    searcherCancelButton->setEnabled(true);
    searcherProgress->setRange(0, 10000);
    searcherProgress->setValue(0);
    searcherProgress->setFormat(QStringLiteral("%p%"));
    auto *searcher = new UndergroundSeedSearcher(
        startDate, endDate, minDelay, maxDelay, filters, maxWalls, searcherRecoverAllPlates->isChecked(), generator);
    auto *timer = new QTimer(this);
    connect(searcherCancelButton, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        searcherCancelButton->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer] {
        searcherModel->addRows(searcher->getResults());
        const int detailedProgress = searcher->getDetailedProgress();
        searcherProgress->setValue(detailedProgress);
        if (!searcher->isSearching())
        {
            timer->stop();
            searcherModel->addRows(searcher->getResults());
            if (!searcher->isCancelled())
            {
                searcherProgress->setValue(searcherProgress->maximum());
            }
            searcherSearchButton->setEnabled(true);
            searcherCancelButton->setEnabled(false);
            delete searcher;
            timer->deleteLater();
        }
    });
    searcher->start();
    timer->start(250);
}

void Underground4::chain()
{
    chainerModel->setRows({});
    chainerPreview->setResult({});
    updateItemNames(chainerItemNames, {});
    chainerRecovered->setText(QStringLiteral("0"));
    if (!currentProfile || chainerItem->currentIndex() < 0) return;

    const u64 startSeed = chainerSeed->getULong();
    const u8 target = static_cast<u8>(chainerItem->currentData().toInt());
    const u8 spots = static_cast<u8>(chainerMaxSpots->value());
    chainerSearchButton->setEnabled(false);
    chainerCancelButton->setEnabled(true);
    chainerProgress->setRange(0, 10000);
    chainerProgress->setValue(0);

    auto *searcher = new UndergroundChainerSearcher(startSeed, target, spots, chainerRecoverAllPlates->isChecked(),
                                                    chainerTemporaryPlates, getGenerator());
    auto *timer = new QTimer(this);
    connect(chainerCancelButton, &QPushButton::clicked, timer, [this, searcher] {
        searcher->cancelSearch();
        chainerCancelButton->setEnabled(false);
    });
    connect(timer, &QTimer::timeout, this, [this, searcher, timer] {
        chainerProgress->setValue(searcher->getDetailedProgress());
        if (!searcher->isSearching())
        {
            timer->stop();
            const auto results = searcher->getResults();
            if (!results.empty())
            {
                chainerModel->setRows(results.front().rows);
                chainerRecovered->setText(QString::number(results.front().recovered));
                if (chainerModel->rowCount() > 0)
                {
                    chainerTable->selectRow(0);
                    chainerProgress->setValue(chainerProgress->maximum());
                }
            }
            chainerSearchButton->setEnabled(true);
            chainerCancelButton->setEnabled(false);
            delete searcher;
            timer->deleteLater();
        }
    });
    searcher->start();
    timer->start(100);
}

void Underground4::generateTimes()
{
    std::vector<UndergroundTimeRow> rows;
    if (searcherStartDate->date() > searcherEndDate->date() || searcherMinDelay->getUInt() > searcherMaxDelay->getUInt())
    {
        searcherTimeModel->setRows(std::move(rows));
        return;
    }

    const u64 targetSeed = searcherSeed->getULong();
    const u32 minDelay = searcherMinDelay->getUInt();
    const u32 maxDelay = searcherMaxDelay->getUInt();
    for (u32 delay = minDelay;; delay++)
    {
        if (targetSeed >= delay)
        {
            u64 value = targetSeed - delay;
            const int second = value % 64; value /= 64;
            const int minute = value % 64; value /= 64;
            const int hour = value % 32; value /= 32;
            const int day = value % 32; value /= 32;
            const int month = value % 16; value /= 16;
            if (value < 100 && second < 60 && minute < 60 && hour < 24)
            {
                for (int year = searcherStartDate->date().year(); year <= searcherEndDate->date().year(); year++)
                {
                    if (year % 100 != static_cast<int>(value)) continue;
                    const QDate date(year, month, day);
                    if (date.isValid() && date >= searcherStartDate->date() && date <= searcherEndDate->date())
                        rows.push_back({ date, QTime(hour, minute, second), delay });
                }
            }
        }
        if (delay == maxDelay) break;
    }
    std::ranges::sort(rows, [](const UndergroundTimeRow &left, const UndergroundTimeRow &right) {
        return left.date < right.date || (left.date == right.date && left.time < right.time);
    });
    searcherTimeModel->setRows(std::move(rows));
}

UndergroundGenerator Underground4::getGenerator(bool includeTemporaryPlates) const
{
    std::array<bool, 16> plates = {};
    for (int plate : minedPlates->getRawCheckedData())
    {
        if (plate >= 0 && plate < static_cast<int>(plates.size())) plates[plate] = true;
    }
    if (includeTemporaryPlates)
        for (u8 plate = 0; plate < 16; plate++)
            if (generatorTemporaryPlates & (1 << plate)) plates[plate] = true;
    return UndergroundGenerator(currentProfile->getVersion(), currentProfile->getTID(), currentProfile->getNationalDex(), plates,
                                currentProfile->getUndergroundOrb());
}

void Underground4::updatePlateText()
{
    const size_t checked = minedPlates->getRawCheckedData().size();
    if (checked == 0) minedPlates->setText(tr("None"));
    else if (checked == plateNames.size()) minedPlates->setText(tr("All"));
    else
    {
        QStringList selected;
        for (int plate : minedPlates->getRawCheckedData()) selected.append(tr(plateNames[plate]));
        minedPlates->setText(minedPlates->fontMetrics().elidedText(selected.join(QStringLiteral(", ")), Qt::ElideRight,
                                                                minedPlates->width() - 20));
    }
}

void Underground4::updateItemFilters()
{
    if (!currentProfile) return;
    const auto available = getGenerator().getAvailableItems();
    std::vector<std::pair<QString, u8>> namedItems;
    namedItems.reserve(available.size());
    for (u8 item : available)
    {
        if (item == 11) continue; // Oval Stone is defined internally but cannot be generated normally.
        namedItems.emplace_back(QString::fromStdString(UndergroundGenerator::getItemName(item)), item);
    }
    std::ranges::sort(namedItems, [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.first, right.first) < 0;
    });
    for (size_t filterIndex = 0; filterIndex < itemFilters.size(); filterIndex++)
    {
        QComboBox *filter = itemFilters[filterIndex];
        const int selected = filter->currentData().toInt();
        filter->blockSignals(true);
        filter->clear();
        filter->addItem(tr("Any"), 0);
        addUndergroundFilterCategories(filter);
        for (const auto &[name, item] : namedItems) filter->addItem(name, item);
        const int index = filter->findData(selected);
        filter->setCurrentIndex(index >= 0 ? index : 0);
        filter->blockSignals(false);
        updateUndergroundFilterCount(filter, itemFilterCounts[filterIndex]);
    }
}

void Underground4::updateSearcherItemFilters()
{
    if (!currentProfile) return;
    const auto available = getGenerator().getAvailableItems();
    std::vector<std::pair<QString, u8>> namedItems;
    namedItems.reserve(available.size());
    for (u8 item : available)
    {
        if (item == 11) continue; // Keep dormant Oval Stone data out of user-facing filters.
        namedItems.emplace_back(QString::fromStdString(UndergroundGenerator::getItemName(item)), item);
    }
    std::ranges::sort(namedItems, [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.first, right.first) < 0;
    });
    for (size_t filterIndex = 0; filterIndex < searcherItemFilters.size(); filterIndex++)
    {
        QComboBox *filter = searcherItemFilters[filterIndex];
        const int selected = filter->currentData().toInt();
        filter->blockSignals(true);
        filter->clear();
        filter->addItem(tr("Any"), 0);
        addUndergroundFilterCategories(filter);
        for (const auto &[name, item] : namedItems) filter->addItem(name, item);
        const int index = filter->findData(selected);
        filter->setCurrentIndex(index >= 0 ? index : 0);
        filter->blockSignals(false);
        updateUndergroundFilterCount(filter, searcherItemFilterCounts[filterIndex], 4 * searcherMaxWalls->value());
    }
}

void Underground4::updateChainerItems()
{
    if (!currentProfile) return;
    const int selected = chainerItem->currentData().toInt();
    const auto available = getGenerator().getAvailableItems();
    std::vector<std::pair<QString, u8>> namedItems;
    for (u8 item : available)
    {
        if (item == 11) continue;
        namedItems.emplace_back(QString::fromStdString(UndergroundGenerator::getItemName(item)), item);
    }
    std::ranges::sort(namedItems, [](const auto &left, const auto &right) {
        return QString::localeAwareCompare(left.first, right.first) < 0;
    });
    chainerItem->blockSignals(true);
    chainerItem->clear();
    chainerItem->addItem(tr("Any Plate"), filterAnyPlate);
    for (const auto &[name, item] : namedItems) chainerItem->addItem(name, item);
    const int index = chainerItem->findData(selected);
    chainerItem->setCurrentIndex(index >= 0 ? index : 0);
    chainerItem->blockSignals(false);
}

void Underground4::applyItemFilters()
{
    if (!advanceCacheValid) return;
    std::array<std::pair<u8, u8>, 4> filters = {};
    for (size_t index = 0; index < filters.size(); index++)
    {
        filters[index] = { static_cast<u8>(itemFilters[index]->currentData().toInt()),
                           static_cast<u8>(itemFilterCounts[index]->value()) };
    }
    advanceModel->applyFilters(filters, onlyTargetAdvances->isChecked() && !targetRoute.empty(),
                               targetRoute.empty() ? 0 : targetRoute.front());
    updateTargetAdvance();
}

void Underground4::updateTargetAdvance()
{
    if (targetRoute.empty())
    {
        advances->clearTargetAdvance();
        return;
    }
    const int row = advanceModel->getRowForActualAdvance(targetRoute.front());
    if (row < 0) advances->clearTargetAdvance();
    else advances->setTargetAdvance(advanceModel->index(row, 0).data().toUInt());
}

void Underground4::generateFrame(bool current)
{
    if (!currentProfile) return;
    UndergroundGenerator generator = getGenerator(true);
    QSpinBox *frame = current ? currentFrame : nextFrame;
    const UndergroundResult result = generator.generate(UndergroundGenerator::advanceSeed(seed->getULong(), frame->value()));
    MiningField4 *field = current ? currentField : nextField;
    field->setResult(result);
    updateItemNames(current ? currentItemNames : nextItemNames, result);
    if (current)
    {
        currentItemCount = result.itemCount;
        for (u8 i = 0; i < requiredItems.size(); i++) requiredItems[i]->setEnabled(i < currentItemCount);
    }
}

void Underground4::populateAdvances(const UndergroundGenerator &generator)
{
    const u32 currentAdvance = static_cast<u32>(currentFrame->value());
    const UndergroundResult first
        = generator.generate(UndergroundGenerator::advanceSeed(seed->getULong(), currentAdvance));
    MiningPlanner planner(first);

    std::map<u32, AdvanceGuidance> guidance;
    u8 requiredMask = 0;
    for (u8 item = 0; item < first.itemCount; item++)
    {
        if (requiredItems[item]->isChecked()) requiredMask |= 1 << item;
    }
    const bool preferCollapse = outcomePriority->currentIndex() == 0;
    auto consider = [&guidance, &planner, requiredMask, preferCollapse](u32 advance, const StrikeState &state, bool collapsed,
                        u8 recoveredMask, u8 itemCount) {
        if ((recoveredMask & requiredMask) != requiredMask) return;
        if (!planner.canRecover(recoveredMask, state.pickaxes, state.hammers)) return;
        AdvanceGuidance candidate;
        candidate.sequence = formatStrikeSequence(state.sequence);
        candidate.collapsed = collapsed;
        candidate.recoveredCount = std::popcount(recoveredMask);
        candidate.switches = state.switches;
        candidate.maxRun = state.maxRun;
        candidate.strikes = state.pickaxes + state.hammers;
        QStringList recovered;
        for (u8 item = 0; item < itemCount; item++)
        {
            if (recoveredMask & (1 << item))
                recovered.append(QString::number(item + 1));
        }
        candidate.recovered = recovered.isEmpty() ? QStringLiteral("-") : recovered.join(QStringLiteral(", "));
        auto it = guidance.find(advance);
        if (it == guidance.end() || betterGuidance(candidate, it->second, preferCollapse)) guidance[advance] = std::move(candidate);
    };

    using StateKey = std::tuple<u32, u8, u8>;
    std::array<std::map<StateKey, StrikeState>, 197> states;
    states[0].emplace(StateKey(0, 2, 0), StrikeState());
    u8 sphereCount = 0;
    for (u8 item = 0; item < first.itemCount; item++)
    {
        if (first.items[item] >= 1 && first.items[item] <= 10) sphereCount++;
    }

    for (u16 damage = 0; damage < 196; damage++)
    {
        for (const auto &[key, state] : states[damage])
        {
            for (u8 tool = 0; tool < 2; tool++)
            {
                StrikeState next = state;
                next.damage = std::min<u16>(196, damage + (tool == 1 ? 8 : 4));
                next.sequence += next.sequence.isEmpty() ? (tool == 1 ? QStringLiteral("H") : QStringLiteral("P"))
                                                        : (tool == 1 ? QStringLiteral(", H") : QStringLiteral(", P"));
                if (state.lastTool == tool) next.lastRun++;
                else
                {
                    if (state.lastTool != 2) next.switches++;
                    next.lastRun = 1;
                }
                next.lastTool = tool;
                next.maxRun = std::max(next.maxRun, next.lastRun);
                if (tool == 1) next.hammers++;
                else next.pickaxes++;

                const u8 duration = static_cast<u8>(next.damage / 15);
                const u32 finalShake = duration > 1 ? 2 : 0;
                const u32 finalBase = state.advances + finalShake;
                const u8 allMask = static_cast<u8>((1 << first.itemCount) - 1);
                const u32 allAdvance = finalBase + 6 * first.itemCount + sphereCount;
                consider(allAdvance, next, false, allMask, first.itemCount);

                if (next.damage == 196)
                {
                    for (u8 mask = 0; mask < allMask; mask++)
                    {
                        u8 recoveredSpheres = 0;
                        for (u8 item = 0; item < first.itemCount; item++)
                        {
                            if ((mask & (1 << item)) && first.items[item] >= 1 && first.items[item] <= 10) recoveredSpheres++;
                        }
                        const u32 collapseAdvance = finalBase + 88 + 6 * std::popcount(mask) + recoveredSpheres;
                        consider(collapseAdvance, next, true, mask, first.itemCount);
                    }
                    continue;
                }

                const u8 maximumShakeFrames = duration > 1 ? duration - 1 : 0;
                StrikeState continued = next;
                continued.advances = state.advances + 2 * maximumShakeFrames;
                StateKey nextKey(continued.advances, continued.lastTool, continued.hammers);
                auto [it, inserted] = states[continued.damage].emplace(nextKey, continued);
                if (!inserted)
                {
                    const int nextStrikes = continued.sequence.count(QLatin1Char(',')) + 1;
                    const int oldStrikes = it->second.sequence.count(QLatin1Char(',')) + 1;
                    if (continued.switches < it->second.switches
                        || (continued.switches == it->second.switches && continued.maxRun > it->second.maxRun)
                        || (continued.switches == it->second.switches && continued.maxRun == it->second.maxRun
                            && nextStrikes < oldStrikes))
                    {
                        it->second = std::move(continued);
                    }
                }
            }
        }
    }

    std::vector<UndergroundAdvanceRow> rows;
    rows.reserve(guidance.size());
    for (const auto &[betweenAdvance, guide] : guidance)
    {
        const u32 actualAdvance = currentAdvance + first.advances + betweenAdvance;
        const UndergroundResult result = generator.generate(UndergroundGenerator::advanceSeed(seed->getULong(), actualAdvance));
        UndergroundAdvanceRow value { 0, actualAdvance, guide.sequence, guide.recovered,
            guide.collapsed ? tr("Wall Collapsed") : tr("All Item Recovered"), guide.collapsed, {}, result.items, result.itemCount };
        for (int item = 0; item < 4; item++)
        {
            value.items[item] = item < result.itemCount ? QString::fromStdString(UndergroundGenerator::getItemName(result.items[item]))
                                                        : QStringLiteral("-");
        }
        rows.emplace_back(std::move(value));
    }
    advanceModel->setAllRows(std::move(rows));
    advanceCacheValid = true;
    applyItemFilters();
}

void Underground4::generate()
{
    if (!currentProfile) return;
    if (advanceCacheValid)
    {
        applyItemFilters();
        return;
    }
    UndergroundGenerator generator = getGenerator(true);
    const u32 currentAdvance = static_cast<u32>(currentFrame->value());
    const UndergroundResult current = generator.generate(UndergroundGenerator::advanceSeed(seed->getULong(), currentAdvance));
    currentItemCount = current.itemCount;
    for (u8 i = 0; i < requiredItems.size(); i++) requiredItems[i]->setEnabled(i < currentItemCount);
    currentField->setResult(current);
    updateItemNames(currentItemNames, current);
    populateAdvances(generator);
    const UndergroundResult empty;
    nextField->setResult(empty);
    updateItemNames(nextItemNames, empty);
}
