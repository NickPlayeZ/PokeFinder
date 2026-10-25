#ifndef UNDERGROUND4_HPP
#define UNDERGROUND4_HPP

#include <Core/Gen4/Tools/UndergroundGenerator.hpp>
#include <QWidget>
#include <array>
#include <vector>

class ComboMenu;
class DateEdit;
class Profile4;
class ProfileDisplay4;
class TableView;
class UndergroundAdvanceModel;
class UndergroundFinderModel;
class UndergroundSearcherModel;
class UndergroundTimeModel;
class QCheckBox;
class QComboBox;
class QLabel;
class QMouseEvent;
class QPoint;
class QPushButton;
class QProgressBar;
class QSpinBox;
class QTimeEdit;
class TextBox;

class MiningField4 final : public QWidget
{
public:
    explicit MiningField4(QWidget *parent = nullptr);
    void setResult(const UndergroundResult &result);
    void setDisplayMode(u8 mode);
    QSize minimumSizeHint() const override;
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    UndergroundResult result;
    u8 displayMode = 1;
};

class DirtField4 final : public QWidget
{
public:
    explicit DirtField4(QWidget *parent = nullptr);
    const std::array<u8, 130> &getDirt() const;
    void reset();
protected:
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
private:
    int tileAt(const QPoint &position) const;
    void paintTile(int index);
    std::array<u8, 130> dirt;
    std::array<bool, 130> painted;
    u8 paintDepth = 2;
    bool painting = false;
};

class Underground4 final : public QWidget
{
    Q_OBJECT
signals:
    void profilesChanged(int);
public:
    explicit Underground4(QWidget *parent = nullptr);
    ~Underground4() override;
public slots:
    void updateProfiles();
private slots:
    void profileChanged(const Profile4 &profile);
    void generate();
    void find();
    void search();
    void generateTimes();
    void platesChanged();
private:
    void generateFrame(bool current);
    UndergroundGenerator getGenerator() const;
    void populateAdvances(const UndergroundGenerator &generator);
    void updateItemFilters();
    void updateSearcherItemFilters();
    void applyItemFilters();
    void updatePlateText();
    ProfileDisplay4 *profileDisplay;
    TextBox *seed;
    ComboMenu *minedPlates;
    QComboBox *outcomePriority;
    std::array<QCheckBox *, 4> requiredItems;
    std::array<QComboBox *, 4> itemFilters;
    std::array<QSpinBox *, 4> itemFilterCounts;
    MiningField4 *currentField;
    MiningField4 *nextField;
    QLabel *currentItemNames;
    QLabel *nextItemNames;
    QSpinBox *currentFrame;
    QSpinBox *nextFrame;
    TableView *advances;
    UndergroundAdvanceModel *advanceModel;
    QPushButton *generateButton;
    DirtField4 *finderField;
    MiningField4 *finderPreview;
    DateEdit *finderDate;
    QTimeEdit *finderTime;
    TextBox *finderTargetDelay;
    TextBox *finderDelayRange;
    QComboBox *finderItemCount;
    QSpinBox *finderMaxTileMismatches;
    UndergroundFinderModel *finderModel;
    DateEdit *searcherStartDate;
    DateEdit *searcherEndDate;
    TextBox *searcherMinDelay;
    TextBox *searcherMaxDelay;
    QPushButton *searcherSearchButton;
    QPushButton *searcherCancelButton;
    QProgressBar *searcherProgress;
    std::array<QComboBox *, 4> searcherItemFilters;
    std::array<QSpinBox *, 4> searcherItemFilterCounts;
    UndergroundSearcherModel *searcherModel;
    TableView *searcherTable;
    MiningField4 *searcherPreview;
    TextBox *searcherSeed;
    UndergroundTimeModel *searcherTimeModel;
    const Profile4 *currentProfile;
    std::vector<u32> miningFieldHistory;
    u8 currentItemCount;
    bool advanceCacheValid;
};

#endif // UNDERGROUND4_HPP
