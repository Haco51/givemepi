#pragma once

#include "storage/ChunkTypes.hpp"
#include "storage/ChunkCodec.hpp"
#include "storage/StoragePolicy.hpp"
#include "storage/StorageTiming.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <set>
#include <unordered_map>

namespace pi::storage
{

/**
 * @brief Local Filesystem 기반 Chunk Store
 * 
 * P/Q/T 중간 결과를 로컬 파일시스템에 저장하고 조회하는 저장소입니다.
 * OS swap 에 의존하지 않고 명시적인 spill/reload 계층을 제공합니다.
 * 
 * 핵심 원칙:
 * - OS swap 에 의존하지 않음
 * - 계산 알고리즘과 파일 I/O 를 분리
 * - 체크포인트 codec 과 CRC32C 를 재사용
 * - 저장된 P/Q/T 를 다시 merge 가능한 형태로 복원
 */
class ChunkStore
{
public:
    /**
     * @brief ChunkStore 생성자
     * @param policy 저장소 정책
     * @param timing 선택적 단계별 timing accumulator
     * @throws std::invalid_argument 정책이 유효하지 않은 경우
     */
    explicit ChunkStore(const StoragePolicy& policy, StorageTiming* timing = nullptr);

    /**
     * @brief ChunkStore 복사 생성자 (move-only class)
     */
    ChunkStore(const ChunkStore&) = delete;
    ChunkStore& operator=(const ChunkStore&) = delete;

    /**
     * @brief ChunkStore 이동 생성자
     */
    ChunkStore(ChunkStore&&) noexcept = default;
    ChunkStore& operator=(ChunkStore&&) noexcept = default;

    /**
     * @brief 저장소 디렉토리
     */
    [[nodiscard]] const std::filesystem::path& directory() const noexcept;

    /**
     * @brief 메모리 예산
     */
    [[nodiscard]] std::uint64_t memoryBudget() const noexcept;

    /**
     * @brief 타겟 chunk 크기
     */
    [[nodiscard]] std::uint64_t targetChunkSize() const noexcept;

    /**
     * @brief Compression 알고리즘
     */
    [[nodiscard]] CompressionAlgorithm compression() const noexcept;

    /**
     * @brief Chunk 저장
     * 
     * @param metadata chunk 메타데이터
     * @param data chunk 데이터 (uncompressed)
     * @return 저장된 chunk ID
     * 
     * - temporary file 로 작성
     * - checksum 계산
     * - compression 적용 (선택적)
     * - atomic rename 로 최종 파일 생성
     * - directory fsync
     */
    ChunkId store(const Chunk& chunk);

    /**
     * @brief Chunk 조회
     * 
     * @param chunkId 조회할 chunk ID
     * @return chunk 데이터가 있는 경우 optional 값, 없는 경우 empty
     */
    std::optional<Chunk> load(ChunkId chunkId) const;

    /**
     * @brief Chunk 존재 확인
     * @param chunkId 확인할 chunk ID
     * @return 존재하는 경우 true, 없는 경우 false
     */
    bool contains(ChunkId chunkId) const;

    /**
     * @brief Chunk 파일 경로 반환
     * @param chunkId 경로가 필요한 chunk ID
     * @return 파일 경로
     */
    [[nodiscard]] std::filesystem::path chunkPath(ChunkId chunkId) const;

    /**
     * @brief Chunk 명시적 제거
     * 
     * @param chunkId 제거할 chunk ID
     * @return 제거 성공 여부
     */
    bool remove(ChunkId chunkId);

    /**
     * @brief Chunk 파일 크기 조회
     * @param chunkId 크기 조회할 chunk ID
     * @return 파일 크기 (bytes)
     */
    std::optional<std::uint64_t> fileSize(ChunkId chunkId) const;

    /**
     * @brief 모든 chunk 파일 조회
     * @return chunk 파일 경로와 크기의 목록
     */
    std::vector<std::pair<std::filesystem::path, std::uint64_t>> listAllFiles() const;

    /**
     * @brief Temporary file 목록 조회
     * @return temporary 파일 경로 목록
     */
    std::vector<std::filesystem::path> listTemporaryFiles() const;

    /**
     * @brief Temporary 파일 정리
     */
    void cleanupTemporaryFiles();

    /**
     * @brief Chunk reload 및 무결성 검증
     * 
     * 저장된 chunk 파일을 다시 로드하고, 다음을 검증합니다:
     * - header 와 version 검사
     * - computation identity 검사
     * - range 와 level 검사
     * - checksum 검사
     * - canonical GMP decode
     * 
     * @param chunkId reload 할 chunk ID
     * @return 검증된 chunk 데이터
     * @throws std::runtime_error 검증 실패 시
     */
    std::optional<Chunk> reloadAndVerify(ChunkId chunkId) const;

    /**
     * @brief Chunk 파일 무결성 검증
     * 
     * @param chunkId 검증할 chunk ID
     * @return 검증 성공 여부
     * @throws std::runtime_error 검증 실패 시
     */
    bool verifyChunkIntegrity(ChunkId chunkId) const;

    /**
     * @brief 모든 chunk 파일 무결성 검증
     * 
     * @return 검증 결과 (성공/실패 chunk 목록)
     */
    std::vector<std::pair<ChunkId, bool>> verifyAllChunks() const;

    /**
     * @brief 손상된 chunk 파일 제거
     * 
     * @return 제거된 chunk 개수
     */
    std::size_t removeCorruptedFiles() const;

    /**
     * @brief MemoryBudget 조회 및 업데이트
     * 
     * @param residentChunks 메모리에 resident 된 chunk ID 목록 (uncompressed size)
     * @param storedChunks 디스크에 저장된 chunk ID 목록 (stored size)
     * @return 메모리 예산 정보
     */
    MemoryBudget queryBudget(
        const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
        const std::vector<std::pair<ChunkId, std::uint64_t>>& storedChunks
    ) const;

    /**
     * @brief Eviction 계획 생성
     * 
     * budget 에 따라 evict 해야 하는 chunk 를 계획합니다.
     * 
     * @param residentChunks 메모리 내 resident chunk 목록
     *        {chunkId, uncompressedSize}
     * @param residentSet 현재 메모리에 실제로 resident 된 chunk ID set
     * @param mergeDistanceMap 각 chunk 가 다음 merge 에 필요한지 표시하는 map
     *        {chunkId, mergeDistance}
     * @param neededBytes budget 을 충족시키기 위해 추가로 evict 해야 하는 bytes
     * @return eviction 계획
     */
    EvictionPlan planEvictions(
        const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
        const std::set<ChunkId>& residentSet,
        const std::unordered_map<ChunkId, double>& mergeDistanceMap,
        std::uint64_t neededBytes
    ) const;

    /**
     * @brief eviction 후보 목록 조회
     * 
     * eviction 우선순위로 정렬된 후보 목록을 반환합니다.
     * 
     * @param residentChunks 메모리 내 resident chunk 목록
     * @param mergeDistanceMap 각 chunk 의 merge distance
     * @param budget 예산
     * @return eviction 후보 목록 (evictionScore 순 정렬)
     */
    std::vector<EvictionCandidate> getEvictionCandidates(
        const std::vector<std::pair<ChunkId, std::uint64_t>>& residentChunks,
        const std::unordered_map<ChunkId, double>& mergeDistanceMap,
        std::uint64_t budget
    ) const;

private:
    /**
     * @brief 저장된 chunk 파일의 CRC32C 계산
     * @param path chunk 파일 경로
     * @return CRC32C 값
     */
    /**
     * @brief Compression 적용
     * @param data 압축할 데이터
     * @return 압축된 데이터
     */
    /**
     * @brief Compression 해제
     * @param data 압축 해제할 데이터
     * @return 원본 데이터
     */
    /**
     * @brief Atomic rename 실행
     * @param source 원본 파일 경로
     * @param target 목표 파일 경로
     * @return 성공 여부
     */
    bool atomicRename(const std::filesystem::path& source, const std::filesystem::path& target);

    /**
     * @brief Directory fsync
     */
    void syncDirectory();

    /**
     * @brief File sync
     * @param path 동기화할 파일 경로
     */
    void syncFile(const std::filesystem::path& path);

    /**
     * @brief Temporary 파일 생성
     * @return temporary 파일 경로
     */
    [[nodiscard]] std::filesystem::path createTemporaryFile(const std::filesystem::path& target) const;

    /**
     * @brief Temporary 파일 삭제
     * @param path temporary 파일 경로
     */
    void removeTemporaryFile(const std::filesystem::path& path);

    /**
     * @brief Chunk 파일에서 ChunkMetadata 파싱
     * @param path chunk 파일 경로
     * @return 파싱된 메타데이터
     * @throws std::runtime_error 파싱 실패 시
     */
    StoragePolicy policy_;
    StorageTiming* timing_ = nullptr;
};

} // namespace pi::storage
