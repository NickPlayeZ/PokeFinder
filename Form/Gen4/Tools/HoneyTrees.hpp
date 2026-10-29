#ifndef HONEYTREES_HPP
#define HONEYTREES_HPP

#include <Core/Global.hpp>
#include <QWidget>
#include <array>

class ComboBox;
class CheckList;
class HoneyTreeModel;
class HoneyTreeSearchControls;
class HoneyTreeSearchModel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class Profile4;
class ProfileDisplay4;
class QCheckBox;
class QComboBox;
class TableView;
class TextBox;

class HoneyTrees final : public QWidget
{
    Q_OBJECT
signals:
    void profilesChanged(int);

public:
    explicit HoneyTrees(QWidget *parent = nullptr);
    ~HoneyTrees() override;
    bool hasProfiles() const;

public slots:
    void updateProfiles();

private slots:
    void profileChanged(const Profile4 &profile);
    void speciesChanged(int index);
    void generate();
    void search();
    void updateNPCNoise();

private:
    void updateLocations();
    void updateSlotLabels();

    ProfileDisplay4 *profileDisplay;
    TextBox *seed;
    TextBox *initialAdvances;
    TextBox *maxAdvances;
    ComboBox *location;
    ComboBox *species;
    QCheckBox *sameTree;
    QComboBox *previousGroup;
    std::array<CheckList *, 3> slotFilters;
    CheckList *shakeFilter;
    QLineEdit *npcNoise;
    TableView *table;
    HoneyTreeModel *model;
    HoneyTreeSearchControls *searchControls;
    HoneyTreeSearchModel *searchModel;
    TextBox *searchMinDelay;
    TextBox *searchMaxDelay;
    TextBox *searchMinAdvance;
    TextBox *searchMaxAdvance;
    QSpinBox *searchConsecutive;
    QCheckBox *searchDelayParity;
    QPushButton *searchButton;
    QPushButton *searchCancelButton;
    QProgressBar *searchProgress;
    const Profile4 *currentProfile;
    bool targetSpeciesActive;
    u16 targetSpecies;
};

#endif // HONEYTREES_HPP
