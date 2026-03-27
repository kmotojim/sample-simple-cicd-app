# Sample Simple CI/CD App

**最小構成の CI/CD デモ** です。  
C++ で書かれたシンプルな HTTP サーバーを、**Tekton Pipeline** でビルド・デプロイします。

## 概要

```
sample-simple-cicd-app/
├── README.md              ← このファイル
├── .gitignore
├── CMakeLists.txt         ← ビルド設定
├── Containerfile          ← コンテナイメージ定義
├── src/
│   └── main.cpp           ← アプリケーション（約100行）
└── manifests/
    ├── namespace.yaml     ← Namespace
    ├── deployment.yaml    ← Deployment
    ├── service.yaml       ← Service
    ├── route.yaml         ← Route
    ├── pipeline.yaml      ← Tekton Pipeline（3タスク）
    └── pipelinerun.yaml   ← 手動実行テンプレート
```

### sample-cicd-app との違い

| 項目 | sample-cicd-app | **sample-simple-cicd-app** |
|------|----------------|---------------------------|
| アプリ | REST API + Web UI (3層設計) | Hello World HTTP サーバー |
| 外部依存 | cpp-httplib, nlohmann-json, googletest | **なし** |
| リポジトリ | ソースとマニフェストが別 | **1つに統合** |
| Pipeline | 7 タスク + Webhook 自動起動 | **3 タスク + 手動起動** |
| CD | ArgoCD (dev/prod 2環境) | **Pipeline から直接デプロイ (1環境)** |
| Kustomize | base + overlays | **不使用 (プレーンYAML)** |

### CI/CD フロー

```
コード変更 → git push → oc create -f pipelinerun.yaml
                              ↓
                         [git-clone]
                              ↓
                        [build-image]  ← Containerfile で CMake ビルド + イメージ作成
                              ↓
                          [deploy]     ← oc set image + oc rollout restart
                              ↓
                        ブラウザで確認!
```

---

## 前提条件

| 項目 | 説明 |
|------|------|
| OpenShift クラスタ | 4.12 以上 |
| OpenShift Pipelines | Operator インストール済み (Tekton) |
| ミラーレジストリ | 非接続環境用のコンテナレジストリ |
| Gitea | ソースコード管理サーバー |
| `oc` CLI | ログイン済み |

### ミラーレジストリに必要なイメージ

以下のイメージがミラーレジストリに存在することを確認してください。

| イメージ | 用途 |
|---------|------|
| `devspaces/udi-rhel9:latest` | Containerfile のビルドステージ (gcc/cmake がプリインストールされたRH イメージとして使用。DevSpaces 自体は不要) |
| `ubi9/ubi:latest` | Containerfile のランタイムステージ |
| `openshift4/ose-cli:latest` | Pipeline の deploy タスク |

---

## セットアップ手順

### Step 1: プレースホルダーの置換

全ファイルに含まれるプレースホルダーを、お使いの環境に合わせて置き換えます。

| プレースホルダー | 説明 | 例 |
|-----------------|------|-----|
| `<MIRROR_REGISTRY>` | ミラーレジストリのホスト名 | `registry.example.com` |
| `<NAMESPACE>` | OpenShift プロジェクト名 | `simple-cicd` |
| `<GITEA_HOST>` | Gitea のホスト名 | `gitea.apps.example.com` |
| `<GITEA_OWNER>` | Gitea のオーナー (ユーザー/組織) | `user1` |

**一括置換コマンド:**

```bash
# リポジトリのルートで実行
MIRROR_REGISTRY="registry.example.com"
NAMESPACE="simple-cicd"
GITEA_HOST="gitea.apps.example.com"
GITEA_OWNER="user1"

# macOS の場合
find . -name '*.yaml' -o -name '*.yml' -o -name 'Containerfile' | \
  xargs sed -i '' \
    -e "s|<MIRROR_REGISTRY>|${MIRROR_REGISTRY}|g" \
    -e "s|<NAMESPACE>|${NAMESPACE}|g" \
    -e "s|<GITEA_HOST>|${GITEA_HOST}|g" \
    -e "s|<GITEA_OWNER>|${GITEA_OWNER}|g"

# Linux の場合
find . -name '*.yaml' -o -name '*.yml' -o -name 'Containerfile' | \
  xargs sed -i \
    -e "s|<MIRROR_REGISTRY>|${MIRROR_REGISTRY}|g" \
    -e "s|<NAMESPACE>|${NAMESPACE}|g" \
    -e "s|<GITEA_HOST>|${GITEA_HOST}|g" \
    -e "s|<GITEA_OWNER>|${GITEA_OWNER}|g"
```

### Step 2: Gitea にリポジトリを作成してプッシュ

```bash
# Gitea にリポジトリを作成した後
git remote set-url origin https://<GITEA_HOST>/<GITEA_OWNER>/sample-simple-cicd-app.git
git add -A
git commit -m "Initial commit"
git push -u origin main
```

### Step 3: アプリケーションのマニフェストを適用

```bash
# OpenShift にログイン
oc login https://<API_SERVER>:6443

# Namespace を作成
oc apply -f manifests/namespace.yaml

# アプリのリソースを作成
oc apply -f manifests/deployment.yaml
oc apply -f manifests/service.yaml
oc apply -f manifests/route.yaml
```

> **Note:** 初回はまだイメージが存在しないため、Pod は ImagePullBackOff になります。
> パイプライン実行後にイメージがプッシュされ、正常にデプロイされます。

### Step 4: Tekton Pipeline を作成

```bash
oc apply -f manifests/pipeline.yaml
```

---

## パイプラインの実行

### 方法 A: YAML ファイルで実行

```bash
oc create -f manifests/pipelinerun.yaml
```

> `oc create` を使います（`oc apply` ではありません）。  
> `generateName` により毎回新しい PipelineRun が作成されます。

### 方法 B: tkn コマンドで実行

```bash
tkn pipeline start simple-cicd-pipeline \
  -p git-url="https://<GITEA_HOST>/<GITEA_OWNER>/sample-simple-cicd-app.git" \
  -p git-revision="main" \
  -p image-name="<MIRROR_REGISTRY>/sample-simple-cicd-app/hello-server" \
  -w name=shared-workspace,volumeClaimTemplateFile=/dev/stdin \
  -n <NAMESPACE> \
  --showlog <<EOF
{"spec":{"accessModes":["ReadWriteOnce"],"resources":{"requests":{"storage":"1Gi"}}}}
EOF
```

### パイプラインの実行状況を確認

```bash
# 最新の PipelineRun のログを表示
tkn pipelinerun logs -f -n <NAMESPACE>

# PipelineRun の一覧
tkn pipelinerun list -n <NAMESPACE>

# または oc コマンドで確認
oc get pipelinerun -n <NAMESPACE>
```

---

## 動作確認

パイプラインが成功したら、アプリにアクセスします。

```bash
# Route の URL を取得
oc get route hello-server -n <NAMESPACE> -o jsonpath='{.spec.host}'

# ヘルスチェック
curl -sk https://$(oc get route hello-server -n <NAMESPACE> -o jsonpath='{.spec.host}')/health
# => {"status":"ok","uptime":42}
```

ブラウザで Route の URL を開くと、"Hello from C++ on OpenShift!" のページが表示されます。

---

## コードを変更して CI/CD を体験する

CI/CD のサイクルを体験するために、コードを少し変更してみましょう。

### 1. コードを変更

`src/main.cpp` の HTML 部分を編集します。例えば、タイトルを変更:

```cpp
    <h1>Hello from C++ on OpenShift!</h1>
```

を以下に変更:

```cpp
    <h1>Hello CI/CD World!</h1>
```

### 2. 変更をプッシュ

```bash
git add -A
git commit -m "Update title"
git push origin main
```

### 3. パイプラインを再実行

```bash
oc create -f manifests/pipelinerun.yaml
```

### 4. 結果を確認

```bash
# パイプラインのログを確認
tkn pipelinerun logs -f -n <NAMESPACE>

# 完了後、ブラウザでリロードすると変更が反映されています!
```

---

## トラブルシューティング

### パイプラインが失敗する場合

```bash
# 失敗した PipelineRun の詳細を確認
tkn pipelinerun describe <pipelinerun-name> -n <NAMESPACE>

# 各タスクのログを確認
tkn taskrun logs <taskrun-name> -n <NAMESPACE>
```

### Pod が起動しない場合

```bash
# Pod のステータスを確認
oc get pods -n <NAMESPACE>

# Pod のイベントを確認
oc describe pod <pod-name> -n <NAMESPACE>

# Pod のログを確認
oc logs <pod-name> -n <NAMESPACE>
```

### よくある問題

| 症状 | 原因 | 対処 |
|------|------|------|
| `ImagePullBackOff` | ミラーレジストリにイメージがない | パイプラインを実行してイメージをビルド |
| `git-clone` タスク失敗 | Gitea の URL が間違い / 到達不能 | プレースホルダーの置換を確認 |
| `buildah` タスク失敗 | ミラーレジストリへの push 権限なし | レジストリの認証設定を確認 |
| `deploy` タスク失敗 | ServiceAccount の権限不足 | `pipeline` SA の RoleBinding を確認 |

---

## 次のステップ

このシンプルな CI/CD を体験した後、さらに学ぶには:

1. **テストの追加** - Pipeline にビルド・テストタスクを追加
2. **Webhook 自動起動** - EventListener + TriggerBinding で git push 時に自動実行
3. **ArgoCD による GitOps** - マニフェストを別リポジトリに分離し、ArgoCD で管理
4. **環境分離** - dev/prod の Kustomize overlay を作成

完全版は [sample-cicd-app](../sample-cicd-app/) と [sample-cicd-app-manifests](../sample-cicd-app-manifests/) を参照してください。
