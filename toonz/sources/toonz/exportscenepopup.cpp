

#include "exportscenepopup.h"

// Tnz6 includes
#include "tapp.h"
#include "filebrowser.h"
#include "iocommand.h"

// TnzQt includes
#include "toonzqt/gutil.h"

// TnzLib includes
#include "toonz/tproject.h"
#include "toonz/toonzscene.h"
#include "toonz/levelset.h"
#include "toonz/sceneresources.h"
#include "toonz/txshlevel.h"
#include "toonz/txshsimplelevel.h"

// TnzCore includes
#include "tsystem.h"

// Qt includes
#include <QVBoxLayout>
#include <QLabel>
#include <QButtonGroup>
#include <QRadioButton>
#include <QPushButton>
#include <QHeaderView>
#include <QPainter>
#include <QApplication>
#include <QMainWindow>
#include <QStandardPaths>

#include <vector>

using namespace DVGui;

TFilePath getStdDocumentsPath() {
  QString documentsPath =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation)[0];
  return TFilePath(documentsPath);
}

//------------------------------------------------------------------------
namespace {
//------------------------------------------------------------------------

TFilePath loadSceneToProject(TFilePath scenePath, ToonzScene &scene) {
  bool ret;
  try {
    ret = IoCmd::loadScene(scene, scenePath, true);
  } catch (TException &e) {
    DVGui::error(QObject::tr("Error loading scene %1 :%2")
                     .arg(toQString(scenePath))
                     .arg(QString::fromStdWString(e.getMessage())));

    return TFilePath();
  } catch (...) {
    DVGui::error(
        QObject::tr("Error loading scene %1").arg(toQString(scenePath)));
    return TFilePath();
  }

  if (!ret) {
    DVGui::error(QObject::tr("It is not possible to export the scene %1 "
                             "because it does not belong to any project.")
                     .arg(toQString(scenePath)));

    return TFilePath();
  }

  TFilePath path = scene.getScenePath();
  DvDirModel::instance()->refreshFolder(
      TProjectManager::instance()->getCurrentProjectPath().getParentDir());
  return path;
}

//------------------------------------------------------------------------

int collectAssets(ToonzScene &scene) {
  ResourceCollector collector(&scene);
  SceneResources resources(&scene, scene.getXsheet());
  resources.accept(&collector);
  int count = collector.getCollectedResourceCount();
  return count;
}

//------------------------------------------------------------------------

void decodeLevelsPath(ToonzScene& scene) {
    std::vector<TXshLevel*> levels;
    scene.getLevelSet()->listLevels(levels);
    for (auto level : levels) {
        TXshSimpleLevel *sl = level->getSimpleLevel();
        if (!sl)continue;
        TFilePath orinPath = sl->getPath();
        if (!orinPath.isAbsolute()){
            TFilePath absolutePath = scene.decodeFilePath(orinPath);
            sl->setPath(absolutePath);
        }
    }
}
}  // namespace
//------------------------------------------------------------------------

//=============================================================================
// MyDvDirModelFileFolderNode [File folder]

DvDirModelNode *ExportSceneDvDirModelFileFolderNode::makeChild(
    std::wstring name) {
  return createExposeSceneNode(this, m_path + name);
}

//-----------------------------------------------------------------------------

DvDirModelFileFolderNode *
ExportSceneDvDirModelFileFolderNode::createExposeSceneNode(
    DvDirModelNode *parent, const TFilePath &path) {
  DvDirModelFileFolderNode *node;
  if (path.getType() == "tnz")
    return 0;
  else if (TProjectManager::instance()->isProject(path))
    node = new ExportSceneDvDirModelProjectNode(parent, path);
  else
    node = new ExportSceneDvDirModelFileFolderNode(parent, path);
  if (path.getName().find("_files") == std::string::npos)
    node->enableRename(true);
  return node;
}

//=============================================================================
// ExportSceneDvDirModelProjectNode

QPixmap ExportSceneDvDirModelProjectNode::getPixmap(bool isOpen) const {
  QPixmap openProjectPixmap =
      createQIcon("folder_project")
          .pixmap(QSize(18, 18), QIcon::Normal, QIcon::On);
  QPixmap closeProjectPixmap =
      createQIcon("folder_project")
          .pixmap(QSize(18, 18), QIcon::Normal, QIcon::Off);

  return isOpen ? openProjectPixmap : closeProjectPixmap;
}

//-----------------------------------------------------------------------------

bool ExportSceneDvDirModelProjectNode::isCurrent() const {
  TProjectManager *pm = TProjectManager::instance();
  return pm->getCurrentProjectPath().getParentDir() == getPath();
}

//-----------------------------------------------------------------------------

DvDirModelFileFolderNode *
ExportSceneDvDirModelProjectNode::createExposeSceneNode(DvDirModelNode *parent,
                                                        const TFilePath &path) {
  DvDirModelFileFolderNode *node;
  if (path.getType() == "tnz")
    return 0;
  else {
    node = new ExportSceneDvDirModelFileFolderNode(parent, path);
    if (path.getName().find("_files") == std::string::npos)
      node->enableRename(true);
    auto project = std::make_shared<TProject>();
    project->load(
        TProjectManager::instance()->projectFolderToProjectPath(getPath()));
    int k = project->getFolderIndexFromPath(node->getPath());
    node->setIsProjectFolder(k >= 0);
  }
  return node;
}

//=============================================================================
// ExportSceneDvDirModelRootNode [Root]

ExportSceneDvDirModelRootNode::ExportSceneDvDirModelRootNode()
    : DvDirModelNode(0, L"Root") {
  m_nodeType = "Root";
}

//-----------------------------------------------------------------------------

void ExportSceneDvDirModelRootNode::add(std::wstring name,
                                        const TFilePath &path) {
  DvDirModelNode *child =
      new ExportSceneDvDirModelFileFolderNode(this, name, path);
  child->setRow((int)m_children.size());
  m_children.push_back(child);
}

//-----------------------------------------------------------------------------

void ExportSceneDvDirModelRootNode::refreshChildren() {
  m_childrenValid = true;
  m_children.clear();
  // if(m_children.empty())
  //{
  TProjectManager *pm = TProjectManager::instance();
  std::vector<TFilePath> projectRoots;
  pm->getProjectRoots(projectRoots);

  int i;
  for (i = 0; i < (int)projectRoots.size(); i++) {
    TFilePath projectRoot = projectRoots[i];
    std::wstring rootDir  = projectRoot.getWideString();
    ExportSceneDvDirModelSpecialFileFolderNode *projectRootNode =
        new ExportSceneDvDirModelSpecialFileFolderNode(
            this, L"Project root (" + rootDir + L")", projectRoot);
    projectRootNode->setPixmap(
        createQIcon("folder_project_root")
            .pixmap(QSize(18, 18), QIcon::Normal, QIcon::Off));
    m_projectRootNodes.push_back(projectRootNode);
    addChild(projectRootNode);
  }

  TFilePath sandboxProjectPath = pm->getSandboxProjectFolder();
  m_sandboxProjectNode =
      new ExportSceneDvDirModelProjectNode(this, sandboxProjectPath);
  addChild(m_sandboxProjectNode);

  // SVN Repository
  QList<SVNRepository> repositories =
      VersionControl::instance()->getRepositories();
  int count = repositories.size();
  for (int i = 0; i < count; i++) {
    SVNRepository repo = repositories.at(i);

    ExportSceneDvDirModelSpecialFileFolderNode *node =
        new ExportSceneDvDirModelSpecialFileFolderNode(
            this, repo.m_name.toStdWString(),
            TFilePath(repo.m_localPath.toStdWString()));
    node->setPixmap(QPixmap(svgToPixmap(":Resources/vcroot.svg")));
    addChild(node);
  }
  //}
}

//-----------------------------------------------------------------------------

DvDirModelNode *ExportSceneDvDirModelRootNode::getNodeByPath(
    const TFilePath &path) {
  DvDirModelNode *node = 0;
  int i;
  //! path could be the sandbox project or in the sandbox project
  if (m_sandboxProjectNode && m_sandboxProjectNode->getPath() == path)
    return m_sandboxProjectNode;
  if (m_sandboxProjectNode) {
    for (i = 0; i < m_sandboxProjectNode->getChildCount(); i++) {
      DvDirModelNode *node =
          m_sandboxProjectNode->getChild(i)->getNodeByPath(path);
      if (node) return node;
    }
  }
  //! or it could be a different project, under some project root
  for (i = 0; i < (int)m_projectRootNodes.size(); i++) {
    node = m_projectRootNodes[i]->getNodeByPath(path);
    if (node) return node;
  }
  //! it could be a regular folder, somewhere in the file system

  return 0;
}

//=============================================================================
// ExportSceneDvDirModel

ExportSceneDvDirModel::ExportSceneDvDirModel() {
  m_root = new ExportSceneDvDirModelRootNode();
  m_root->refreshChildren();
}

//-----------------------------------------------------------------------------

ExportSceneDvDirModel::~ExportSceneDvDirModel() { delete m_root; }

//-----------------------------------------------------------------------------

DvDirModelNode *ExportSceneDvDirModel::getNode(const QModelIndex &index) const {
  if (index.isValid())
    return static_cast<DvDirModelNode *>(index.internalPointer());
  else
    return m_root;
}

//-----------------------------------------------------------------------------

QModelIndex ExportSceneDvDirModel::index(int row, int column,
                                         const QModelIndex &parent) const {
  if (column != 0) return QModelIndex();
  DvDirModelNode *parentNode = m_root;
  if (parent.isValid()) parentNode = getNode(parent);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *node = parentNode->getChild(row);
  return createIndex(row, column, node);
}

//-----------------------------------------------------------------------------

QModelIndex ExportSceneDvDirModel::parent(const QModelIndex &index) const {
  if (!index.isValid()) return QModelIndex();
  DvDirModelNode *node       = getNode(index);
  DvDirModelNode *parentNode = node->getParent();
  if (!parentNode || parentNode == m_root)
    return QModelIndex();
  else
    return createIndex(parentNode->getRow(), 0, parentNode);
}

//-----------------------------------------------------------------------------

QModelIndex ExportSceneDvDirModel::childByName(const QModelIndex &parent,
                                               const std::wstring &name) const {
  if (!parent.isValid()) return QModelIndex();
  DvDirModelNode *parentNode = getNode(parent);
  if (!parentNode) return QModelIndex();
  int row = parentNode->rowByName(name);
  if (row < 0 || row >= parentNode->getChildCount()) return QModelIndex();
  DvDirModelNode *childNode = parentNode->getChild(row);
  return createIndex(row, 0, childNode);
}

//-----------------------------------------------------------------------------

int ExportSceneDvDirModel::rowCount(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  int childCount       = node->getChildCount();
  return childCount;
}

//-----------------------------------------------------------------------------

QVariant ExportSceneDvDirModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid()) return QVariant();
  DvDirModelNode *node = getNode(index);
  if (role == Qt::DisplayRole || role == Qt::EditRole)
    return QString::fromStdWString(node->getName());
  else if (role == Qt::DecorationRole) {
    return QVariant();
  } else if (role == Qt::ForegroundRole) {
    if (!node || !node->isRenameEnabled())
      return QBrush(Qt::blue);
    else
      return QVariant();
  } else
    return QVariant();
}

//-----------------------------------------------------------------------------

Qt::ItemFlags ExportSceneDvDirModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags flags = QAbstractItemModel::flags(index);
  if (index.isValid()) {
    DvDirModelNode *node = getNode(index);
    if (node && node->isRenameEnabled()) flags |= Qt::ItemIsEditable;
  }
  return flags;
}

//-----------------------------------------------------------------------------

bool ExportSceneDvDirModel::setData(const QModelIndex &index,
                                    const QVariant &value, int role) {
  if (!index.isValid()) return false;
  DvDirModelNode *node = getNode(index);
  if (!node || !node->isRenameEnabled()) return false;
  QString newName = value.toString();
  if (newName == "") return false;
  if (!node->setName(newName.toStdWString())) return false;
  emit dataChanged(index, index);
  return true;
}

//-----------------------------------------------------------------------------

bool ExportSceneDvDirModel::hasChildren(const QModelIndex &parent) const {
  DvDirModelNode *node = getNode(parent);
  return node->hasChildren();
}

//-----------------------------------------------------------------------------

void ExportSceneDvDirModel::refresh(const QModelIndex& index) {
    DvDirModelNode* node = index.isValid() ? getNode(index) : m_root;
    if (!node) return;
    int oldCount = node->getChildCount();
    if (oldCount > 0) {
        emit beginRemoveRows(index, 0, oldCount - 1);
        node->removeChildren(0, oldCount);
        emit endRemoveRows();
    }
    node->refreshChildren();
    int newCount = node->getChildCount();
    if (newCount > 0) {
        emit beginInsertRows(index, 0, newCount - 1);
        emit endInsertRows();
    }
}

//=============================================================================
// ExportSceneTreeViewDelegate

ExportSceneTreeViewDelegate::ExportSceneTreeViewDelegate(
    ExportSceneTreeView *parent)
    : QItemDelegate(parent), m_treeView(parent) {}

//-----------------------------------------------------------------------------

ExportSceneTreeViewDelegate::~ExportSceneTreeViewDelegate() {}

//-----------------------------------------------------------------------------

void ExportSceneTreeViewDelegate::paint(QPainter *painter,
                                        const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const {
  QRect rect           = option.rect;
  DvDirModelNode *node = DvDirModel::instance()->getNode(index);
  if (!node) return;

  ExportSceneDvDirModelProjectNode *pnode =
      dynamic_cast<ExportSceneDvDirModelProjectNode *>(node);
  ExportSceneDvDirModelFileFolderNode *fnode =
      dynamic_cast<ExportSceneDvDirModelFileFolderNode *>(node);

  bool isCurrent = node == m_treeView->getCurrentNode();
  if (isCurrent)
    painter->fillRect(rect.adjusted(-2, 0, 0, 0),
                      QBrush(m_treeView->getSelectedItemBackground()));

  QPixmap px = node->getPixmap(m_treeView->isExpanded(index));
  if (!px.isNull()) {
    int x = rect.left();
    int y = rect.center().y() - ((rect.height() / 2) - 2);
    painter->drawPixmap(QPoint(x, y), px);
  }
  rect.adjust(pnode ? 31 : 22, 0, 0, 0);
  QVariant d   = index.data();
  QString name = QString::fromStdWString(node->getName());
  if (fnode && fnode->isProjectFolder()) {
    painter->setPen((isCurrent) ? m_treeView->getSelectedFolderTextColor()
                                : m_treeView->getFolderTextColor());
  } else {
    painter->setPen((isCurrent) ? m_treeView->getSelectedTextColor()
                                : m_treeView->getTextColor());
  }
  QRect nameBox;
  painter->drawText(rect, Qt::AlignVCenter | Qt::AlignLeft, name, &nameBox);

  if (pnode) {
    painter->setPen(m_treeView->getTextColor());
    if (pnode->isCurrent())
      painter->setBrush(Qt::red);
    else
      painter->setBrush(Qt::NoBrush);
    int d = 8;
    int y = (rect.height() - d) / 2;
    painter->drawEllipse(rect.x() - d - 4, rect.y() + y, d, d);
  }
}

//-----------------------------------------------------------------------------

QSize ExportSceneTreeViewDelegate::sizeHint(const QStyleOptionViewItem &option,
                                            const QModelIndex &index) const {
  return QSize(QItemDelegate::sizeHint(option, index).width() + 10, 22);
}

//=============================================================================
// ExportSceneTreeView

ExportSceneTreeView::ExportSceneTreeView(QWidget *parent)
    : StyledTreeView(parent) {
  setStyleSheet("border:1px solid rgba(0,0,0,0.5);");
  m_model = new ExportSceneDvDirModel();
  setModel(m_model);
  header()->close();
  setItemDelegate(new ExportSceneTreeViewDelegate(this));
  setSelectionMode(QAbstractItemView::SingleSelection);

  // Connect all possible changes that can alter the
  // bottom horizontal scrollbar to resize contents...
  bool ret = connect(this, SIGNAL(expanded(const QModelIndex &)), this,
                     SLOT(resizeToConts()));
  ret      = ret && connect(this, SIGNAL(collapsed(const QModelIndex &)), this,
                            SLOT(resizeToConts()));
  ret      = ret && connect(this->model(), SIGNAL(layoutChanged()), this,
                            SLOT(resizeToConts()));

  assert(ret);
  setAcceptDrops(true);
}

//-----------------------------------------------------------------------------

void ExportSceneTreeView::resizeToConts() { resizeColumnToContents(0); }

//-----------------------------------------------------------------------------

void ExportSceneTreeView::refresh() { m_model->refresh(QModelIndex()); }

//-----------------------------------------------------------------------------

DvDirModelNode *ExportSceneTreeView::getCurrentNode() const {
  QModelIndex index    = currentIndex();
  DvDirModelNode *node = m_model->getNode(index);
  return node;
}

//-----------------------------------------------------------------------------

QSize ExportSceneTreeView::sizeHint() const { return QSize(100, 100); }

//-----------------------------------------------------------------------------

void ExportSceneTreeView::showEvent(QShowEvent *) { refresh(); }

//-----------------------------------------------------------------------------

void ExportSceneTreeView::focusInEvent(QFocusEvent *event) {
  QTreeView::focusInEvent(event);
  emit focusIn();
}

//=============================================================================
// ExportScenePopup

ExportScenePopup::ExportScenePopup(std::vector<TFilePath> scenes)
    : Dialog(TApp::instance()->getMainWindow(), true, false, "ExportScene")
    , m_scenes(scenes)
    , m_mode(0) {
  setWindowTitle(tr("Export Scene"));

  bool ret = true;

  QVBoxLayout *layout = new QVBoxLayout(this);

  //  m_command = new QLabel(this);
  //  layout->addWidget(m_command);

  QButtonGroup *group = new QButtonGroup(this);
  group->setExclusive(true);

  // Choose Project
  QWidget *chooseProjectWidget     = new QWidget(this);
  QVBoxLayout *chooseProjectLayout = new QVBoxLayout(chooseProjectWidget);

  m_chooseProjectButton =
      new QRadioButton(tr("Choose Existing Project"), chooseProjectWidget);
  group->addButton(m_chooseProjectButton, 0);
  m_chooseProjectButton->setChecked(true);
  chooseProjectLayout->addWidget(m_chooseProjectButton);

  m_projectTreeView = new ExportSceneTreeView(chooseProjectWidget);
  m_projectTreeView->setMinimumWidth(400);
  ret = ret && connect(m_projectTreeView, SIGNAL(focusIn()), this,
                       SLOT(onProjectTreeViweFocusIn()));
  chooseProjectLayout->addWidget(m_projectTreeView);

  chooseProjectWidget->setLayout(chooseProjectLayout);
  layout->addWidget(chooseProjectWidget, 5);

  // New Project
  QWidget *newProjectWidget     = new QWidget(this);
  QGridLayout *newProjectLayout = new QGridLayout(newProjectWidget);

  m_newProjectButton =
      new QRadioButton(tr("Create New Project"), newProjectWidget);
  group->addButton(m_newProjectButton, 1);
  newProjectLayout->addWidget(m_newProjectButton, 0, 0, 1, 2, Qt::AlignLeft);

  m_newProjectNameLabel = new QLabel(tr("Name:"), newProjectWidget);
  newProjectLayout->addWidget(m_newProjectNameLabel, 1, 0, 1, 1,
                              Qt::AlignRight);

  m_newProjectName = new LineEdit(newProjectWidget);
  ret              = ret && connect(m_newProjectName, SIGNAL(focusIn()), this,
                                    SLOT(onProjectNameFocusIn()));
  newProjectLayout->setColumnStretch(1, 5);
  newProjectLayout->addWidget(m_newProjectName, 1, 1, 1, 1, Qt::AlignLeft);

  m_newProPathFieldLabel = new QLabel(tr("Create In:"), this);
  m_projectLocationFld =
      new DVGui::FileField(this, getStdDocumentsPath().getQString());
  ret = ret && connect(m_projectLocationFld->getField(), SIGNAL(focusIn()),
                       this, SLOT(onProjectNameFocusIn()));

  newProjectLayout->addWidget(m_newProPathFieldLabel, 2, 0,
                              Qt::AlignRight | Qt::AlignVCenter);
  newProjectLayout->addWidget(m_projectLocationFld, 2, 1);

  newProjectWidget->setLayout(newProjectLayout);
  layout->addWidget(newProjectWidget);

  // Export as Lonely Scene
  QWidget* lonelyProjectWidget = new QWidget(this);
  QGridLayout* lonelyProjectLayout = new QGridLayout(lonelyProjectWidget);

  m_lonelyModeButton =
      new QRadioButton(tr("Export as Standalone Scene"), lonelyProjectWidget);
  group->addButton(m_lonelyModeButton, 2);
  lonelyProjectLayout->addWidget(m_lonelyModeButton, 0, 0, 1, 2);

  m_lonelyModePathLabel = new QLabel(tr("Export To:"), lonelyProjectWidget);
  lonelyProjectLayout->addWidget(m_lonelyModePathLabel, 1, 0, Qt::AlignRight | Qt::AlignVCenter);

  m_lonelyModePathFld =
      new DVGui::FileField(lonelyProjectWidget, getStdDocumentsPath().getQString());
  ret = ret && connect(m_lonelyModePathFld->getField(), SIGNAL(focusIn()),
      this, SLOT(onLonelyModeFocusIn()));
  lonelyProjectLayout->addWidget(m_lonelyModePathFld, 1, 1);

  lonelyProjectWidget->setLayout(lonelyProjectLayout);
  layout->addWidget(lonelyProjectWidget);

  ret = ret &&
      connect(group, SIGNAL(buttonClicked(int)), this, SLOT(switchMode(int)));

  addLayout(layout, false);

  QPushButton *okBtn = new QPushButton(tr("Export"), this);
  okBtn->setDefault(true);
  QPushButton *cancelBtn = new QPushButton(tr("Cancel"), this);
  connect(okBtn, SIGNAL(clicked()), this, SLOT(onExport()));
  connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));

  addButtonBarWidget(okBtn, cancelBtn);

  switchMode(0);
  //  updateCommandLabel();
  
  adjustSize();
  assert(ret);
}

//-----------------------------------------------------------------------------

void ExportScenePopup::switchMode(int id) {
    assert(id < 3);
    m_mode = id;
    // m_projectTreeView->setEnabled(true);
}

//-----------------------------------------------------------------------------

void ExportScenePopup::onProjectTreeViweFocusIn() {
  m_chooseProjectButton->setChecked(true);
  switchMode(0);
}

//-----------------------------------------------------------------------------

void ExportScenePopup::onProjectNameFocusIn() {
  m_newProjectButton->setChecked(true);
  switchMode(1);
}

void ExportScenePopup::onLonelyModeFocusIn()
{
    m_lonelyModeButton->setChecked(true);
    switchMode(2);
}

//-----------------------------------------------------------------------------

void ExportScenePopup::onExport() {
  QApplication::setOverrideCursor(Qt::WaitCursor);
  TProjectManager *pm      = TProjectManager::instance();
  TFilePath oldProjectPath = pm->getCurrentProjectPath();
  TFilePath projectPath;
  if (m_mode == 0) {// Export to Existing Project
      DvDirModelFileFolderNode* node =
          (DvDirModelFileFolderNode*)m_projectTreeView->getCurrentNode();
      if (!node || node->getNodeType() == "Root" || !pm->isProject(node->getPath())) {
          QApplication::restoreOverrideCursor();
          DVGui::warning(tr("The folder you selected is not a project."));
          return;
      }
      projectPath = pm->projectFolderToProjectPath(node->getPath());
      assert(projectPath != TFilePath());
  }
  else if (m_mode == 1) {// Create New Project
      projectPath = createNewProject();
      if (projectPath == TFilePath()) {
          QApplication::restoreOverrideCursor();
          return;
      }
  }
  else // Export as Lonely Scene
      projectPath = TFilePath(); pm->getSandboxProjectPath();

  pm->setCurrentProjectPath(projectPath);

  int i;
  QString label("Exporting Scenes");
  QString cancel("Cancel");
  ProgressDialog progressBar(label, cancel, 0, m_scenes.size(), this);
  progressBar.show();
  for (i = 0; i < m_scenes.size(); i++) {
    ToonzScene scene;
    TFilePath newScenePath = loadSceneToProject(m_scenes[i], scene);
    if (m_mode == 2)decodeLevelsPath(scene);
    if (newScenePath == TFilePath()) {
        DVGui::warning(tr("There was an error exporting the scene."));
        continue;
    };
    progressBar.setValue(i);
    label = QString("Exporting ") +
        QString::fromStdString(newScenePath.getName());
    if (m_mode == 2) {
        QString path = m_lonelyModePathFld->getPath();
        TFilePath newSceneFolder = TFilePath(path) + newScenePath.getName();
        TSystem::mkDir(newSceneFolder);
        newScenePath = newSceneFolder + m_scenes[i].withoutParentDir();
        scene.setScenePath(newScenePath);
    };
    int count = collectAssets(scene);
    scene.save(newScenePath);
  }
  pm->setCurrentProjectPath(oldProjectPath);

  QApplication::restoreOverrideCursor();
  accept();
}

//-----------------------------------------------------------------------------

TFilePath ExportScenePopup::createNewProject() {
  TProjectManager *pm = TProjectManager::instance();
  TFilePath projectName(m_newProjectName->text().toStdWString());
  if (projectName == TFilePath()) {
    DVGui::warning(
        tr("The project name cannot be empty or contain any of the following "
           "characters:(new line)   \\ / : * ? \"  |"));
    return TFilePath();
  }
  if (projectName.isAbsolute()) {
    // bad project name
    DVGui::warning(
        tr("The project name cannot be empty or contain any of the following "
           "characters:(new line)   \\ / : * ? \"  |"));
    return TFilePath();
  }
  if (pm->getProjectPathByName(projectName) != TFilePath()) {
    // project already exists
    DVGui::warning(tr("The project name you specified is already used."));
    return TFilePath();
  }

  TFilePath newLocation   = TFilePath(m_projectLocationFld->getPath());
  TFilePath projectFolder = newLocation + projectName;
  TFilePath projectPath   = pm->projectFolderToProjectPath(projectFolder);

  if (TSystem::doesExistFileOrLevel(projectPath)) {
    error(tr("Project '%1' already exists").arg(m_newProjectName->text()));
    return TFilePath();
  }

  auto project = std::make_shared<TProject>();

  auto currentProject = pm->getCurrentProject();
  assert(currentProject);
  int i;
  for (i = 0; i < (int)currentProject->getFolderCount(); i++)
    project->setFolder(currentProject->getFolderName(i),
                       currentProject->getFolder(i));
  project->save(projectPath);
  DvDirModel::instance()->refreshFolder(newLocation);

  return projectPath;
}

//-----------------------------------------------------------------------------
/*
void ExportScenePopup::updateCommandLabel()
{
  if(m_scenes.empty())
    return;
  int sceneCount= m_scenes.size();
  if(sceneCount==1)
    m_command->setText(tr("Stai esportando la scena selezionata nel seguente
progetto:"));
  else
    m_command->setText(tr("Stai esportando ") + QString::number(sceneCount) +
tr(" scene nel seguente progetto:"));
}
*/
